#include "utils.h"

#define DATA_SIZE 1024*5
#define BLOCK_SIZE 1024

static struct rdma_cm_id *cm_server_id = NULL;
static struct rdma_event_channel *cm_event_channel = NULL;
static struct ibv_qp_init_attr qp_init_attr; // client queue pair attributes

/* RDMA memory resources */
static struct ibv_mr *client_metadata_mr = NULL, *server_buffer_mr = NULL, *server_metadata_mr = NULL;

static struct ibv_recv_wr client_recv_wr, *bad_client_recv_wr = NULL;
static struct ibv_send_wr server_send_wr, *bad_server_send_wr = NULL;
static struct ibv_sge client_recv_sge, server_send_sge;


static struct exchange_buffer server_buff, client_buff;


// client resources struct
struct per_client_resources {
    struct ibv_pd *pd;
    struct ibv_cq *cq;
    struct ibv_comp_channel *completion_channel;
    struct ibv_qp *qp;
    struct rdma_cm_id *client_id;
    unsigned long *table_start_addr;
};

// connection struct
struct per_connection_struct {
    char* memory_region;
    struct ibv_mr *memory_region_mr;
};

static struct per_client_resources *client_res = NULL;

static void setup_client_resources(struct rdma_cm_id *cm_client_id)
{
    client_res = (struct per_client_resources*) malloc(sizeof(struct per_client_resources));
    if(!cm_client_id){
        error("Client id is still NULL \n");
        return;
    }
    client_res->client_id = cm_client_id;

    HANDLE(client_res->pd = ibv_alloc_pd(cm_client_id->verbs));
    info("Protection domain (PD) allocated: %p \n", client_res->pd);

    HANDLE(client_res->completion_channel = ibv_create_comp_channel(cm_client_id->verbs));
    info("I/O completion event channel created: %p \n",
         client_res->completion_channel);

    HANDLE(client_res->cq = ibv_create_cq(cm_client_id->verbs,
                       CQ_CAPACITY,
                       NULL,
                       client_res->completion_channel,
                       0));
    info("Completion queue (CQ) created: %p with %d elements \n",
         client_res->cq, client_res->cq->cqe);

    /* Ask for the event for all activities in the completion queue*/
    HANDLE_NZ(ibv_req_notify_cq(client_res->cq,
                            0));
    bzero(&qp_init_attr, sizeof qp_init_attr);
    qp_init_attr.cap.max_recv_sge = MAX_SGE; /* Maximum SGE per receive posting */
    qp_init_attr.cap.max_recv_wr = MAX_WR; /* Maximum receive posting capacity */
    qp_init_attr.cap.max_send_sge = MAX_SGE; /* Maximum SGE per send posting */
    qp_init_attr.cap.max_send_wr = MAX_WR; /* Maximum send posting capacity */
    qp_init_attr.qp_type = IBV_QPT_RC; /* QP type, RC = Reliable connection */
    qp_init_attr.recv_cq = client_res->cq;
    qp_init_attr.send_cq = client_res->cq;
    HANDLE_NZ(rdma_create_qp(client_res->client_id,
                         client_res->pd,
                         &qp_init_attr ));
    client_res->qp = cm_client_id->qp;

    info("Client QP created: %p \n", client_res->qp);
}


static int start_rdma_server(struct sockaddr_in *server_socket_addr) {
    // create an event channel
    HANDLE(cm_event_channel = rdma_create_event_channel());
    // create server id
    HANDLE_NZ(rdma_create_id(cm_event_channel, &cm_server_id, NULL, RDMA_PS_TCP));
    // bind server id to the given socket credentials
    HANDLE_NZ(rdma_bind_addr(cm_server_id, (struct sockaddr*) server_socket_addr));
    int ret;
    // listen
    ret = rdma_listen(cm_server_id, 8);
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

static void accept_conn(struct rdma_cm_id *cm_client_id) {
    struct rdma_conn_param conn_param;
    memset(&conn_param, 0, sizeof(conn_param));
    conn_param.initiator_depth = 3; /* this tell how many outstanding requests can we handle */
    conn_param.responder_resources = 3; /* This tell how many outstanding requests we expect other side to handle */

    // Now accept connection from the client.
    HANDLE_NZ(rdma_accept(cm_client_id, &conn_param));
    info("Wait for : RDMA_CM_EVENT_ESTABLISHED event \n");
}

// Receive request for client to send the offset
static int post_recv_offset(struct per_connection_struct* conn) {

    client_buff.message = malloc(sizeof(struct msg));
    HANDLE(client_buff.buffer = rdma_buffer_register(client_res->pd,
                                                       client_buff.message,
                                                       sizeof(struct msg),
                                                       (IBV_ACCESS_LOCAL_WRITE)));

    client_recv_sge.addr = (uint64_t)client_buff.buffer->addr;
    client_recv_sge.length = client_buff.buffer->length;
    client_recv_sge.lkey = client_buff.buffer->lkey;

    bzero(&client_recv_wr, sizeof(client_recv_wr));
    client_recv_wr.sg_list = &client_recv_sge;
    client_recv_wr.num_sge = 1; // only one SGE

    HANDLE_NZ(ibv_post_recv(client_res->qp,
                            &client_recv_wr,
                            &bad_client_recv_wr));

    info("Receive buffer for client offset pre-posting is successful \n");
}

static void build_memory_map(struct per_connection_struct *conn) {

    conn->memory_region = malloc( DATA_SIZE  + (8 * (DATA_SIZE / BLOCK_SIZE)));
    memset(conn->memory_region, 0, DATA_SIZE  + (8 * (DATA_SIZE / BLOCK_SIZE)));

    conn->memory_region_mr = rdma_buffer_register(client_res->pd,
                                                     conn->memory_region,
                                                     DATA_SIZE  + (8 * (DATA_SIZE / BLOCK_SIZE)),
                                                     (IBV_ACCESS_LOCAL_WRITE|
                                                      IBV_ACCESS_REMOTE_READ|
                                                      IBV_ACCESS_REMOTE_WRITE));

}

static void post_send_memory_map(struct per_connection_struct* conn) {
    struct sockaddr_in remote_sockaddr;
    struct ibv_wc wc;

    /* Extract client socket information */
    memcpy(&remote_sockaddr /* where to save */,
           rdma_get_peer_addr(client_res->client_id) /* gives you remote sockaddr */,
           sizeof(struct sockaddr_in) /* max size */);
    printf("A new connection is accepted from %s \n",
           inet_ntoa(remote_sockaddr.sin_addr));

    // Receive the client metadata -
    HANDLE(process_work_completion_events(client_res->completion_channel, &wc, 1));
    show_exchange_buffer(client_buff.message);


//    if (client_buff->message->type == OFFSET) {
//
//        server_buff->message = malloc(sizeof(struct msg));
//        memset(server_buff->message, 0, sizeof(struct msg));
//        server_buff->message->type = ADDRESS;
//        memcpy(&server_buff->message->data.mr, conn->memory_region_mr, sizeof(struct ibv_mr));
//        server_buff->message->data.mr->addr = (void *)(conn->memory_region);
//
//        HANDLE(server_buff->buffer = rdma_buffer_register(client_res->pd,
//                                                          server_buff->message,
//                                                           sizeof(struct msg),
//                                                           (IBV_ACCESS_LOCAL_WRITE)));
//
//        server_send_sge.addr = (uint64_t) server_buff->message;
//        server_send_sge.length = sizeof(struct msg);
//        server_send_sge.lkey = server_buff->buffer->lkey;
//
//        bzero(&server_send_wr, sizeof(server_send_wr));
//        server_send_wr.sg_list = &server_send_sge;
//        server_send_wr.num_sge = 1;
//        server_send_wr.opcode = IBV_WR_SEND;
//        server_send_wr.send_flags = IBV_SEND_SIGNALED;
//
//        // memory map should have been initiated already. send that memory_map address
//        HANDLE_NZ(ibv_post_send(client_res->qp, &server_send_wr, &bad_server_send_wr));
//        HANDLE(process_work_completion_events(client_res->completion_channel, &wc, 1));
//    }
}

static int disconnect_and_cleanup(struct per_connection_struct* conn)
{
    struct rdma_cm_event *cm_event = NULL;
    int ret = -1;
    rdma_ack_cm_event(cm_event);

    printf("A disconnect event is received from the client...\n");
    /* We free all the resources */
    /* Destroy QP */
    rdma_destroy_qp(client_res->client_id);

    /* Destroy CQ */
    ret = ibv_destroy_cq(client_res->cq);
    if (ret) {
        error("Failed to destroy completion queue cleanly, %d \n", -errno);
        // we continue anyways;
    }
    /* Destroy completion channel */
    ret = ibv_destroy_comp_channel(client_res->completion_channel);
    if (ret) {
        error("Failed to destroy completion channel cleanly, %d \n", -errno);
        // we continue anyways;
    }
    /* Destroy memory buffers */
    rdma_buffer_free(server_buffer_mr);
    rdma_buffer_deregister(server_metadata_mr);
    rdma_buffer_deregister(client_metadata_mr);
    /* Destroy rdma server id */
    ret = rdma_destroy_id(cm_server_id);
    if (ret) {
        error("Failed to destroy server id cleanly, %d \n", -errno);
        // we continue anyways;
    }
    rdma_destroy_event_channel(cm_event_channel);
    free(conn);
    free(client_res);

    /* Destroy client cm id */
    ret = rdma_destroy_id(client_res->client_id);
    if (ret) {
        error("Failed to destroy client id cleanly, %d \n", -errno);
        // we continue anyways;
    }
    printf("Server shut-down is complete \n");
    return 0;
}

static void poll_for_completion_events(int num_wc) {
    struct ibv_wc wc;
    HANDLE(process_work_completion_events(client_res->completion_channel, &wc, num_wc));
    show_exchange_buffer(client_buff.message);
}

static int wait_for_event() {
    int ret;

    struct rdma_cm_event *dummy_event = NULL;
    struct per_connection_struct* connection = NULL;
    // when a client connects, it sends a connect request
    while(rdma_get_cm_event(cm_event_channel, &dummy_event) == 0){
        struct rdma_cm_event cm_event;
        memcpy(&cm_event, dummy_event, sizeof(*dummy_event));

        info("%s event received \n", rdma_event_str(cm_event.event));
        switch(cm_event.event) {
            case RDMA_CM_EVENT_CONNECT_REQUEST:
                connection = (struct per_connection_struct*) malloc (sizeof(struct per_connection_struct*));
                rdma_ack_cm_event(dummy_event); //Ack the event
                setup_client_resources(cm_event.id); // send a recv req for client_metadata
//                build_memory_map(connection);
                post_recv_offset(connection);
                accept_conn(cm_event.id);
                break;
            case RDMA_CM_EVENT_ESTABLISHED:
                rdma_ack_cm_event(dummy_event);
                poll_for_completion_events(1);
//                post_send_memory_map(connection);
                break;
            case RDMA_CM_EVENT_DISCONNECTED:
//                disconnect_and_cleanup(connection);
                break;
            default:
                error("Event not found %s", (char *) cm_event.event);
                return -1;
        }
    }
    return ret;
}


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
    return 0;
}