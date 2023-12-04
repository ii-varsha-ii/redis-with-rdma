#include "utils.h"

static struct rdma_cm_id *cm_server_id = NULL;
static struct rdma_event_channel *cm_event_channel = NULL;
static struct ibv_pd *pd = NULL; //protection domain
static struct ibv_comp_channel *io_completion_channel = NULL; // completion channel
static struct ibv_cq *cq = NULL; // completion queue
static struct ibv_qp_init_attr qp_init_attr; // client queue pair attributes


/* RDMA memory resources */
static struct ibv_mr *client_metadata_mr = NULL, *server_buffer_mr = NULL, *server_metadata_mr = NULL;
static struct rdma_buffer_attr client_metadata_attr, server_metadata_attr;
static struct ibv_recv_wr client_recv_wr, *bad_client_recv_wr = NULL;
static struct ibv_send_wr server_send_wr, *bad_server_send_wr = NULL;
static struct ibv_sge client_recv_sge, server_send_sge;

#define HANDLE(x)  do { if (!(x)) error("ERROR: " #x " failed (returned zero/null).\n"); } while (0)
#define HANDLE_NZ(x) do { if ( (x)) error("error: " #x " failed (returned non-zero)." ); } while (0)


struct map_attributes {
    struct rdma_cm_id *id;
    struct ibv_qp *qp;

    char *rdma_remote_region;
};

static int setup_client_resources(struct rdma_cm_id *cm_client_id, struct ibv_qp *client_qp)
{
    int ret = -1;
    if(!cm_client_id){
        error("Client id is still NULL \n");
        return -EINVAL;
    }

    // Allocate protection domain
    HANDLE(pd = ibv_alloc_pd(cm_client_id->verbs));
    info("A new protection domain is allocated at %p \n", pd);

    // create completion channel for I/O completion notifications
    HANDLE(io_completion_channel = ibv_create_comp_channel(cm_client_id->verbs));
    info("An I/O completion event channel is created at %p \n",
          io_completion_channel);

    // create completion queue where I/O completion metadata can be sent. For this we need the completion channel
    HANDLE(cq = ibv_create_cq(cm_client_id->verbs /* which device*/,
                       CQ_CAPACITY /* maximum capacity*/,
                       NULL /* user context, not used here */,
                       io_completion_channel /* which IO completion channel */,
                       0 /* signaling vector, not used here*/));
    info("Completion queue (CQ) is created at %p with %d elements \n",
          cq, cq->cqe);

    /* Ask for the event for all activities in the completion queue*/
    HANDLE_NZ(ibv_req_notify_cq(cq /* on which CQ */,
                            0 /* 0 = all event type, no filter*/));

    bzero(&qp_init_attr, sizeof qp_init_attr);
    qp_init_attr.cap.max_recv_sge = MAX_SGE; /* Maximum SGE per receive posting */
    qp_init_attr.cap.max_recv_wr = MAX_WR; /* Maximum receive posting capacity */
    qp_init_attr.cap.max_send_sge = MAX_SGE; /* Maximum SGE per send posting */
    qp_init_attr.cap.max_send_wr = MAX_WR; /* Maximum send posting capacity */
    qp_init_attr.qp_type = IBV_QPT_RC; /* QP type, RC = Reliable connection */

    /* We use same completion queue, but one can use different queues */
    qp_init_attr.recv_cq = cq;
    qp_init_attr.send_cq = cq;

    HANDLE_NZ(rdma_create_qp(cm_client_id,
                         pd,
                         &qp_init_attr ));

    memcpy(client_qp, cm_client_id->qp, sizeof(*cm_client_id->qp));
    info("Client QP created at %p\n", &client_qp);
    return ret;
}


static int start_rdma_server(struct sockaddr_in *server_socket_addr) {
    int ret;
    // create an event channel
    cm_event_channel = rdma_create_event_channel();
    if (!cm_event_channel) {
        error("[rdma]: Creating cm event channel failed with errno : (%d)", -errno);
        return -errno;
    }

    // create server id
    ret = rdma_create_id(cm_event_channel, &cm_server_id, NULL, RDMA_PS_TCP);
    if (ret) {
        error("Creating server cm id failed with errno: %d ", -errno);
        return -errno;
    }

    // bind server id to the given socket credentials
    ret = rdma_bind_addr(cm_server_id, (struct sockaddr*) server_socket_addr);
    if (ret) {
        error("Failed to bind server address, errno: %d \n", -errno);
        return -errno;
    }

    // listen
    ret = rdma_listen(cm_server_id, 8); /* backlog = 8 clients, same as TCP, see man listen*/
    if (ret) {
        error("rdma_listen failed to listen on server address, errno: %d ",
                   -errno);
        return -errno;
    }

    printf("Server is listening successfully at: %s , port: %d \n",
           inet_ntoa(server_socket_addr->sin_addr),
           ntohs(server_socket_addr->sin_port));
    return ret;
}

/* Pre-posts a receive buffer and accepts an RDMA client connection */
static void build_client_metadata_buffer(struct rdma_cm_id *cm_client_id, struct ibv_qp *client_qp)
{
    struct rdma_cm_event *cm_event = NULL;

    if(!cm_client_id || !client_qp) {
        error("Client resources are not properly setup\n");
        return;
    }

    // Prepare buffer ( allocate memory with local write permissions ) to receive metadata from client
    HANDLE(client_metadata_mr = rdma_buffer_register(pd /* which protection domain */,
                                              &client_metadata_attr /* what memory */,
                                              sizeof(client_metadata_attr) /* what length */,
                                              (IBV_ACCESS_LOCAL_WRITE) /* access permissions */));

    // Create a SGE object with addr, length and lkey
    client_recv_sge.addr = (uint64_t) client_metadata_mr->addr; // same as &client_buffer_attr
    client_recv_sge.length = client_metadata_mr->length;
    client_recv_sge.lkey = client_metadata_mr->lkey;

    // Pass that SGE to the Work Request
    bzero(&client_recv_wr, sizeof(client_recv_wr));
    client_recv_wr.sg_list = &client_recv_sge;
    client_recv_wr.num_sge = 1; // only one SGE

    // Post a Receive request
    HANDLE_NZ(ibv_post_recv(client_qp,
                        &client_recv_wr,
                        &bad_client_recv_wr));
    info("Receive buffer for client metadata pre-posting is successful \n");
}

static void accept_conn(struct rdma_cm_id *cm_client_id) {

    info( "Accepting connections\n" );
    struct rdma_conn_param conn_param;
    memset(&conn_param, 0, sizeof(conn_param));
    conn_param.initiator_depth = 3; /* this tell how many outstanding requests can we handle */
    conn_param.responder_resources = 3; /* This tell how many outstanding requests we expect other side to handle */

    // Now accept connection from the client.
    HANDLE_NZ(rdma_accept(cm_client_id, &conn_param));
    info("Wait for : RDMA_CM_EVENT_ESTABLISHED event \n");
}

#define DATA_SIZE 1024*5
#define BLOCK_SIZE 1024

static void build_memory_map(struct rdma_cm_id *cm_client_id, struct ibv_qp *client_qp) {
    struct map_attributes *attr = (struct map_attributes *) malloc(sizeof (struct map_attributes));
    attr->id = cm_client_id;
    attr->qp = client_qp;
    attr->rdma_remote_region = (char *) malloc ( 1024 * 5 );

    
    for (int i = 0; i<5; i++) {
        attr->rdma_remote_region[i] = malloc()
    }

}

static int accept_connection(struct rdma_cm_id *cm_client_id) {
    struct sockaddr_in remote_sockaddr;
    int ret;
    /* Extract client socket information */
    memcpy(&remote_sockaddr /* where to save */,
           rdma_get_peer_addr(cm_client_id) /* gives you remote sockaddr */,
           sizeof(struct sockaddr_in) /* max size */);
    printf("A new connection is accepted from %s \n",
           inet_ntoa(remote_sockaddr.sin_addr));
}

static int wait_for_event() {
    int ret;

    struct rdma_cm_event *dummy_event = NULL;
    // when a client connects, it sends a connect request
    while(rdma_get_cm_event(cm_event_channel, &dummy_event) == 0){
        struct rdma_cm_event cm_event;
        memcpy(&cm_event, dummy_event, sizeof(*dummy_event));

        info("A new %s type event is received \n", rdma_event_str(cm_event.event));
        switch(cm_event.event) {
            case RDMA_CM_EVENT_CONNECT_REQUEST:
                rdma_ack_cm_event(dummy_event); //Ack the event
                struct ibv_qp client_qp;
                HANDLE(ret = setup_client_resources(cm_event.id, &client_qp));
                build_client_metadata_buffer(cm_event.id, &client_qp);
                build_memory_map(cm_event.id, &client_qp);
                accept_conn(cm_event.id);
                break;
            case RDMA_CM_EVENT_ESTABLISHED:
                rdma_ack_cm_event(dummy_event);
                accept_connection(cm_event.id);
                break;
            default:
                error("Event not found %s", (char *) cm_event.event);
                return -1;
        }
    }
    return ret;
}

static int send_server_metadata_to_client(struct ibv_qp *client_qp)
{
    struct ibv_wc wc;
    int ret = -1;

    // Waiting for client to send its metadata
    ret = process_work_completion_events(io_completion_channel, &wc, 1);
    if (ret != 1) {
        error("Failed to receive , ret = %d \n", ret);
        return ret;
    }

    printf("Client side buffer information is received...\n");
    show_rdma_buffer_attr(&client_metadata_attr);
    printf("The client has requested buffer length of : %u bytes \n",
           client_metadata_attr.length);

    // Create memory buffer to do RDMA reads and writes. The length of the buffer will be given by the client
    server_buffer_mr = rdma_buffer_alloc(pd /* which protection domain */,
                                         client_metadata_attr.length /* what size to allocate */,
                                         (IBV_ACCESS_LOCAL_WRITE|
                                          IBV_ACCESS_REMOTE_READ|
                                          IBV_ACCESS_REMOTE_WRITE) /* access permissions */);
    if(!server_buffer_mr){
        error("Server failed to create a buffer \n");
        /* we assume that it is due to out of memory error */
        return -ENOMEM;
    }

    // Set the addr, length and lkey of the buffer allocated in the server side to the client
    server_metadata_attr.address = (uint64_t) server_buffer_mr->addr;
    server_metadata_attr.length = (uint32_t) server_buffer_mr->length;
    server_metadata_attr.stag.local_stag = (uint32_t) server_buffer_mr->lkey;
    server_metadata_mr = rdma_buffer_register(pd /* which protection domain*/,
                                              &server_metadata_attr /* which memory to register */,
                                              sizeof(server_metadata_attr) /* what is the size of memory */,
                                              IBV_ACCESS_LOCAL_WRITE /* what access permission */);
    if(!server_metadata_mr){
        error("Server failed to create to hold server metadata \n");
        /* we assume that this is due to out of memory error */
        return -ENOMEM;
    }

    // Create a SGE request with the Work Request
    server_send_sge.addr = (uint64_t) &server_metadata_attr;
    server_send_sge.length = sizeof(server_metadata_attr);
    server_send_sge.lkey = server_metadata_mr->lkey;

    // Link SGE with the Send Request
    bzero(&server_send_wr, sizeof(server_send_wr));
    server_send_wr.sg_list = &server_send_sge;
    server_send_wr.num_sge = 1; // only 1 SGE element in the array
    server_send_wr.opcode = IBV_WR_SEND; // This is a send request
    server_send_wr.send_flags = IBV_SEND_SIGNALED; // We want to get notification

    // Post a Send Request
    ret = ibv_post_send(client_qp /* which QP */,
                        &server_send_wr /* Send request that we prepared before */,
                        &bad_server_send_wr /* In case of error, this will contain failed requests */);
    if (ret) {
        error("Posting of server metdata failed, errno: %d \n",
                   -errno);
        return -errno;
    }

    // Waiting for Send Request - completion notification
    ret = process_work_completion_events(io_completion_channel, &wc, 1);
    if (ret != 1) {
        error("Failed to send server metadata, ret = %d \n", ret);
        return ret;
    }
    info("Local buffer metadata has been sent to the client \n");
    return 0;
}

//static int disconnect_and_cleanup()
//{
//    struct rdma_cm_event *cm_event = NULL;
//    int ret = -1;
//    /* Now we wait for the client to send us disconnect event */
//    info("Waiting for cm event: RDMA_CM_EVENT_DISCONNECTED\n");
//    ret = on_event(cm_event_channel,
//                                RDMA_CM_EVENT_DISCONNECTED,
//                                &cm_event);
//    if (ret) {
//        error("Failed to get disconnect event, ret = %d \n", ret);
//        return ret;
//    }
//    /* We acknowledge the event */
//    ret = rdma_ack_cm_event(cm_event);
//    if (ret) {
//        error("Failed to acknowledge the cm event %d\n", -errno);
//        return -errno;
//    }
//    printf("A disconnect event is received from the client...\n");
//    /* We free all the resources */
//    /* Destroy QP */
//    rdma_destroy_qp(cm_client_id);
//    /* Destroy client cm id */
//    ret = rdma_destroy_id(cm_client_id);
//    if (ret) {
//        error("Failed to destroy client id cleanly, %d \n", -errno);
//        // we continue anyways;
//    }
//    /* Destroy CQ */
//    ret = ibv_destroy_cq(cq);
//    if (ret) {
//        error("Failed to destroy completion queue cleanly, %d \n", -errno);
//        // we continue anyways;
//    }
//    /* Destroy completion channel */
//    ret = ibv_destroy_comp_channel(io_completion_channel);
//    if (ret) {
//        error("Failed to destroy completion channel cleanly, %d \n", -errno);
//        // we continue anyways;
//    }
//    /* Destroy memory buffers */
//    rdma_buffer_free(server_buffer_mr);
//    rdma_buffer_deregister(server_metadata_mr);
//    rdma_buffer_deregister(client_metadata_mr);
//    /* Destroy protection domain */
//    ret = ibv_dealloc_pd(pd);
//    if (ret) {
//        error("Failed to destroy client protection domain cleanly, %d \n", -errno);
//        // we continue anyways;
//    }
//    /* Destroy rdma server id */
//    ret = rdma_destroy_id(cm_server_id);
//    if (ret) {
//        error("Failed to destroy server id cleanly, %d \n", -errno);
//        // we continue anyways;
//    }
//    rdma_destroy_event_channel(cm_event_channel);
//    printf("Server shut-down is complete \n");
//    return 0;
//}



int main(int argc, char **argv) {
    // struct for Internet Socket Address
    struct sockaddr_in server_socket_addr;
    // set the struct variable to 0
    bzero(&server_socket_addr, sizeof(server_socket_addr));
    server_socket_addr.sin_family = AF_INET; // IP protocol family
    server_socket_addr.sin_addr.s_addr = htonl(INADDR_ANY); // ANY address
    int ret, option;
    while((option = getopt(argc, argv, "a:p:")) != -1) {
        switch (option) {
            case 'a':
                ret = get_addr(optarg, (struct sockaddr*) &server_socket_addr);
                if (ret) {
                    error("Invalid IP");
                    return ret;
                }
                break;
            case 'p':
                server_socket_addr.sin_port = htons(strtol(optarg, NULL, 0));
                break;
            default:
                break;
        }
    }

    if (!server_socket_addr.sin_port) {
        server_socket_addr.sin_port = htons(DEFAULT_RDMA_PORT);
    }
    ret = start_rdma_server(&server_socket_addr);
    if (ret) {
        error("RDMA server failed to start cleanly, ret = %d \n", ret);
        return ret;
    }

    wait_for_event();

//        ret = send_server_metadata_to_client();
//        if (ret) {
//            error("Failed to send server metadata to the client, ret = %d \n", ret);
//            return ret;
//        }
//
//
//    ret = disconnect_and_cleanup();
//    if (ret) {
//        error("Failed to clean up resources properly, ret = %d \n", ret);
//        return ret;
//    }
    return 0;

}