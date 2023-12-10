#include "utils.h"

static struct rdma_cm_id *cm_server_id = NULL;
static struct rdma_event_channel *cm_event_channel = NULL;
static struct ibv_qp_init_attr qp_init_attr; // client queue pair attributes

static struct ibv_recv_wr client_recv_wr, *bad_client_recv_wr = NULL;
static struct ibv_send_wr server_send_wr, *bad_server_send_wr = NULL;
static struct ibv_sge client_recv_sge, server_send_sge;

static struct exchange_buffer server_buff, client_buff;
static struct per_client_resources *client_res[10];

// client resources struct
struct per_client_resources {
    struct ibv_pd *pd;
    struct ibv_cq *cq;
    struct ibv_comp_channel *completion_channel;
    struct ibv_qp *qp;
    struct rdma_cm_id *client_id;
    struct ibv_mr *memory_region_mr;
};

// connection struct
struct per_memory_struct {
    char* memory_region;
    unsigned long *mapping_table_start;
};

static void setup_client_resources(struct rdma_cm_id *cm_client_id, struct per_memory_struct* conn, int i)
{
    client_res[i] = (struct per_client_resources*) malloc(sizeof(struct per_client_resources));
    if(!cm_client_id){
        error("Client id is still NULL \n");
        return;
    }
    client_res[i]->client_id = cm_client_id;

    HANDLE(client_res[i]->pd = ibv_alloc_pd(cm_client_id->verbs));
    debug("Protection domain (PD) allocated: %p \n", client_res[i]->pd)

    HANDLE(client_res[i]->completion_channel = ibv_create_comp_channel(cm_client_id->verbs));
    debug("I/O completion event channel created: %p \n",
          client_res[i]->completion_channel)

    HANDLE(client_res[i]->cq = ibv_create_cq(cm_client_id->verbs,
                                          CQ_CAPACITY,
                                          NULL,
                                          client_res[i]->completion_channel,
                                          0));
    debug("Completion queue (CQ) created: %p with %d elements \n",
          client_res[i]->cq, client_res[i]->cq->cqe)

    /* Ask for the event for all activities in the completion queue*/
    HANDLE_NZ(ibv_req_notify_cq(client_res[i]->cq,
                                0));
    bzero(&qp_init_attr, sizeof qp_init_attr);
    qp_init_attr.cap.max_recv_sge = MAX_SGE; /* Maximum SGE per receive posting */
    qp_init_attr.cap.max_recv_wr = MAX_WR; /* Maximum receive posting capacity */
    qp_init_attr.cap.max_send_sge = MAX_SGE; /* Maximum SGE per send posting */
    qp_init_attr.cap.max_send_wr = MAX_WR; /* Maximum send posting capacity */
    qp_init_attr.qp_type = IBV_QPT_RC; /* QP type, RC = Reliable connection */
    qp_init_attr.recv_cq = client_res[i]->cq;
    qp_init_attr.send_cq = client_res[i]->cq;
    HANDLE_NZ(rdma_create_qp(client_res[i]->client_id,
                             client_res[i]->pd,
                             &qp_init_attr ));
    client_res[i]->qp = cm_client_id->qp;

    client_res[i]->memory_region_mr = rdma_buffer_register(client_res[i]->pd,
                                                  conn->memory_region,
                                                  DATA_SIZE  + (8 * (DATA_SIZE / BLOCK_SIZE)),
                                                  (IBV_ACCESS_LOCAL_WRITE|
                                                   IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE));

    debug("Client QP created: %p \n", client_res[i]->qp)
}


static void start_rdma_server(struct sockaddr_in *server_socket_addr) {
    HANDLE(cm_event_channel = rdma_create_event_channel());
    HANDLE_NZ(rdma_create_id(cm_event_channel, &cm_server_id, NULL, RDMA_PS_TCP));
    HANDLE_NZ(rdma_bind_addr(cm_server_id, (struct sockaddr*) server_socket_addr));
    HANDLE_NZ(rdma_listen(cm_server_id, 8));

    info("Server is listening successfully at: %s , port: %d \n",
         inet_ntoa(server_socket_addr->sin_addr),
         ntohs(server_socket_addr->sin_port));
}

static void accept_conn(struct rdma_cm_id *cm_client_id) {
    struct rdma_conn_param conn_param;
    memset(&conn_param, 0, sizeof(conn_param));
    conn_param.initiator_depth = 3;
    conn_param.responder_resources = 3;

    HANDLE_NZ(rdma_accept(cm_client_id, &conn_param));
    debug("Wait for : RDMA_CM_EVENT_ESTABLISHED event \n")
}

static void post_recv_offset(int i) {
    client_buff.message = malloc(sizeof(struct msg));
    HANDLE(client_buff.buffer = rdma_buffer_register(client_res[i]->pd,
                                                     client_buff.message,
                                                     sizeof(struct msg),
                                                     (IBV_ACCESS_LOCAL_WRITE)));

    client_recv_sge.addr = (uint64_t)client_buff.buffer->addr;
    client_recv_sge.length = client_buff.buffer->length;
    client_recv_sge.lkey = client_buff.buffer->lkey;

    bzero(&client_recv_wr, sizeof(client_recv_wr));
    client_recv_wr.sg_list = &client_recv_sge;
    client_recv_wr.num_sge = 1; // only one SGE

    HANDLE_NZ(ibv_post_recv(client_res[i]->qp,
                            &client_recv_wr,
                            &bad_client_recv_wr));

    info("Receive buffer for client OFFSET pre-posting is successful \n");
}

static void build_memory_map(struct per_memory_struct *conn) {
    conn->memory_region = malloc( DATA_SIZE  + (8 * (DATA_SIZE / BLOCK_SIZE)));

    info(" Memory Map initialized: \n");
    redisContext *context = redisConnect("127.0.0.1", 6379);
    redisReply* reply;
    for ( int i = 0 ; i < (DATA_SIZE / BLOCK_SIZE); i++) {
        char offset_str[2];
        sprintf(offset_str, "%d", i);
        reply = redisCommand(context, "GET %s", offset_str);
        if (reply == NULL || reply->type != REDIS_REPLY_STRING) {
            strcpy(conn->memory_region + (i * BLOCK_SIZE) + (8 * (DATA_SIZE/ BLOCK_SIZE)), "(nil)");
            freeReplyObject(reply);
            continue;
        }
        strcpy(conn->memory_region + (i * BLOCK_SIZE) + (8 * (DATA_SIZE/ BLOCK_SIZE)), reply->str);
        freeReplyObject(reply);
    }

    print_memory_map(conn->memory_region);

    conn->mapping_table_start = (unsigned long * ) conn->memory_region;
    debug("Initiated memory map: %p\n", conn->mapping_table_start)
}

static void post_send_memory_map(struct per_memory_struct* conn, int i) {
    struct sockaddr_in remote_sockaddr;
    struct ibv_wc wc;

    /* Extract client socket information */
    memcpy(&remote_sockaddr /* where to save */,
           rdma_get_peer_addr(client_res[i]->client_id) /* gives you remote sockaddr */,
           sizeof(struct sockaddr_in) /* max size */);
    info("A new connection is accepted from %s \n",
         inet_ntoa(remote_sockaddr.sin_addr));

    if (client_buff.message->type == OFFSET) {

        server_buff.message = malloc(sizeof(struct msg));
        server_buff.message->type = ADDRESS;

        memcpy(&server_buff.message->data.mr, client_res[i]->memory_region_mr, sizeof(struct ibv_mr));
        server_buff.message->data.mr.addr = (void *)(conn->memory_region);

        server_buff.buffer = rdma_buffer_register(client_res[i]->pd,
                                                  server_buff.message,
                                                  sizeof(struct msg),
                                                  (IBV_ACCESS_LOCAL_WRITE|
                                                   IBV_ACCESS_REMOTE_READ |
                                                   IBV_ACCESS_REMOTE_WRITE));

        info("Sending ADDRESS... \n");
        show_exchange_buffer(server_buff.message);

        server_send_sge.addr = (uint64_t) server_buff.message;
        server_send_sge.length = (uint32_t) sizeof(struct msg);
        server_send_sge.lkey = server_buff.buffer->lkey;

        bzero(&server_send_wr, sizeof(server_send_wr));
        server_send_wr.sg_list = &server_send_sge;
        server_send_wr.num_sge = 1;
        server_send_wr.opcode = IBV_WR_SEND;
        server_send_wr.send_flags = IBV_SEND_SIGNALED;

        // memory map should have been initiated already. send that memory_map address
        HANDLE_NZ(ibv_post_send(client_res[i]->qp, &server_send_wr, &bad_server_send_wr));
        info( "Send request with memory map ADDRESS is successful \n");
    }
}

//static int disconnect_and_cleanup(struct per_connection_struct* conn)
//{
//    struct rdma_cm_event *cm_event = NULL;
//    int ret = -1;
//    rdma_ack_cm_event(cm_event);
//
//    printf("A disconnect event is received from the client...\n");
//    /* We free all the resources */
//    /* Destroy QP */
//    rdma_destroy_qp(client_res->client_id);
//
//    /* Destroy CQ */
//    ret = ibv_destroy_cq(client_res->cq);
//    if (ret) {
//        error("Failed to destroy completion queue cleanly, %d \n", -errno);
//        // we continue anyways;
//    }
//    /* Destroy completion channel */
//    ret = ibv_destroy_comp_channel(client_res->completion_channel);
//    if (ret) {
//        error("Failed to destroy completion channel cleanly, %d \n", -errno);
//        // we continue anyways;
//    }
//    /* Destroy rdma server id */
//    ret = rdma_destroy_id(cm_server_id);
//    if (ret) {
//        error("Failed to destroy server id cleanly, %d \n", -errno);
//        // we continue anyways;
//    }
//    rdma_destroy_event_channel(cm_event_channel);
//    free(conn);
//    free(client_res);
//
//    /* Destroy client cm id */
//    ret = rdma_destroy_id(client_res->client_id);
//    if (ret) {
//        error("Failed to destroy client id cleanly, %d \n", -errno);
//        // we continue anyways;
//    }
//    printf("Server shut-down is complete \n");
//    return 0;
//}

static void poll_for_completion_events(int num_wc, int i) {
    struct ibv_wc wc;
    int total_wc = process_work_completion_events(client_res[i]->completion_channel, &wc, num_wc);

    for (int i = 0 ; i < total_wc; i++) {
        if( (&(wc) + i)->opcode & IBV_WC_RECV ) {
            if ( client_buff.message->type == OFFSET ) {
                show_exchange_buffer(client_buff.message);
            }
        }
    }
}

void* mem_map_print(void *args) {
    struct per_memory_struct* conn = args;
    while(1) {
        sleep(4);
        print_memory_map(conn->memory_region);


    }
}

struct client_mapping {
    struct rdma_cm_event cm_event;
    int i;
};

int getValue() {

}

static int wait_for_event() {
    int ret;

    struct rdma_cm_event *dummy_event = NULL;
//    struct client_mapping *mappings[10] = NULL;
    struct per_memory_struct* connection = NULL;
    pthread_t thread1;
    connection = (struct per_memory_struct*) malloc (sizeof(struct per_memory_struct*));
    build_memory_map(connection);

    int i = 0;
    // when a client connects, it sends a connect request
    while(rdma_get_cm_event(cm_event_channel, &dummy_event) == 0){
        struct rdma_cm_event cm_event;
        memcpy(&cm_event, dummy_event, sizeof(*dummy_event));
        info("%s\n", cm_event.id->verbs->device->name)
        info("%s event received \n", rdma_event_str(cm_event.event));
        switch(cm_event.event) {
            case RDMA_CM_EVENT_CONNECT_REQUEST:
//                mappings[i] = malloc(sizeof(struct client_mapping));
//                mappings->cm_event = cm_event;
//                mappings->i = i;
                rdma_ack_cm_event(dummy_event); //Ack the event
                setup_client_resources(cm_event.id, connection, i); // send a recv req for client_metadata
                post_recv_offset(i);
                accept_conn(cm_event.id);
                i = i + 1;
                break;
            case RDMA_CM_EVENT_ESTABLISHED:
                rdma_ack_cm_event(dummy_event);
                poll_for_completion_events(1, i);
                post_send_memory_map(connection, i);
                poll_for_completion_events(1, i);
                //pthread_create(&thread1, NULL, mem_map_print, (void *) connection);
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
    start_rdma_server(&server_socket_addr);
    wait_for_event();
    return 0;
}