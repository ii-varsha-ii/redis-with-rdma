#include "utils.h"

static struct rdma_event_channel *cm_event_channel = NULL;
static struct rdma_cm_id *cm_client_id = NULL;
static struct ibv_qp_init_attr qp_init_attr; // client queue pair attributes

static struct ibv_send_wr client_send_wr, *bad_client_send_wr = NULL;
static struct ibv_recv_wr server_recv_wr, *bad_server_recv_wr = NULL;
static struct ibv_sge client_send_sge, server_recv_sge;

// connection struct
struct per_connection_struct {
    struct ibv_mr server_mr;
    char* local_memory_region;
    struct ibv_mr *local_memory_region_mr;
};

static void client_prepare_connection(struct sockaddr_in *s_addr) {
    client_res = (struct per_client_resources*) malloc(sizeof(struct per_client_resources));

    // Client side - create event channel
    HANDLE(cm_event_channel = rdma_create_event_channel());
    debug("RDMA CM event channel created: %p \n", cm_event_channel)

    // Create client ID
    HANDLE_NZ(rdma_create_id(cm_event_channel, &cm_client_id,
                             NULL,
                             RDMA_PS_TCP));

    client_res->client_id = cm_client_id;
    // Resolve IP address to RDMA address and bind to client_id
    HANDLE_NZ(rdma_resolve_addr(client_res->client_id, NULL, (struct sockaddr *) s_addr, 2000));
    debug("waiting for cm event: RDMA_CM_EVENT_ADDR_RESOLVED\n")
}

static int setup_client_resources(struct sockaddr_in *s_addr) {
    info("Trying to connect to server at : %s port: %d \n",
           inet_ntoa(s_addr->sin_addr),
           ntohs(s_addr->sin_port));

    // Create a protection domain
    HANDLE(client_res->pd = ibv_alloc_pd(client_res->client_id->verbs));
    debug("Protection Domain (PD) allocated: %p \n", client_res->pd )

    // Create a completion channel
    HANDLE(client_res->completion_channel = ibv_create_comp_channel(cm_client_id->verbs));
    debug("Completion channel created: %p \n", client_res->completion_channel)

    // Create a completion queue pair
    HANDLE(client_res->cq = ibv_create_cq(client_res->client_id->verbs /* which device*/,
                              CQ_CAPACITY /* maximum capacity*/,
                              NULL /* user context, not used here */,
                              client_res->completion_channel /* which IO completion channel */,
                              0 /* signaling vector, not used here*/));
    debug("CQ created: %p with %d elements \n", client_res->cq, client_res->cq->cqe)

    // Receive notifications from complete queue pair
    HANDLE_NZ(ibv_req_notify_cq(client_res->cq, 0));

    bzero(&qp_init_attr, sizeof qp_init_attr);
    qp_init_attr.cap.max_recv_sge = MAX_SGE; /* Maximum SGE per receive posting */
    qp_init_attr.cap.max_recv_wr = MAX_WR; /* Maximum receive posting capacity */
    qp_init_attr.cap.max_send_sge = MAX_SGE; /* Maximum SGE per send posting */
    qp_init_attr.cap.max_send_wr = MAX_WR; /* Maximum send posting capacity */
    qp_init_attr.qp_type = IBV_QPT_RC; /* QP type, RC = Reliable connection */

    /* We use same completion queue, but one can use different queues */
    qp_init_attr.recv_cq = client_res->cq;
    qp_init_attr.send_cq = client_res->cq;

    HANDLE_NZ(rdma_create_qp(client_res->client_id,
                         client_res->pd,
                         &qp_init_attr));

    client_res->qp = cm_client_id->qp;
    debug("Client QP created: %p \n", client_res->qp)
    return 0;
}

static int post_recv_server_memory_map()
{
    server_buff.message = malloc(sizeof(struct msg));
    HANDLE(server_buff.buffer = rdma_buffer_register(client_res->pd,
                                server_buff.message,
                                sizeof(struct msg),
                                (IBV_ACCESS_LOCAL_WRITE)));

    server_recv_sge.addr = (uint64_t) server_buff.message;
    server_recv_sge.length = (uint32_t) sizeof(struct msg);
    server_recv_sge.lkey = server_buff.buffer->lkey;

    bzero(&server_recv_wr, sizeof(server_recv_wr));
    server_recv_wr.sg_list = &server_recv_sge;
    server_recv_wr.num_sge = 1;

    HANDLE_NZ(ibv_post_recv(client_res->qp /* which QP */,
                        &server_recv_wr /* receive work request*/,
                        &bad_server_recv_wr /* error WRs */));
    info("Receive buffer pre-posting is successful \n");
    return 0;
}

static void connect_to_server()
{
    struct rdma_conn_param conn_param;
    bzero(&conn_param, sizeof(conn_param));
    conn_param.initiator_depth = 3;
    conn_param.responder_resources = 3;
    conn_param.retry_count = 3;
    HANDLE_NZ(rdma_connect(client_res->client_id, &conn_param));
}

static int post_send_to_server()
{
    client_buff.message = malloc(sizeof(struct msg));
    client_buff.message->type = OFFSET;
    client_buff.message->data.offset = 0;

    HANDLE(client_buff.buffer = rdma_buffer_register(client_res->pd,
                                        client_buff.message,
                                         sizeof(struct msg),
                                         (IBV_ACCESS_LOCAL_WRITE|
                                          IBV_ACCESS_REMOTE_READ|
                                          IBV_ACCESS_REMOTE_WRITE)));

    info("Sending OFFSET... \n");
    show_exchange_buffer(client_buff.message);

    client_send_sge.addr = (uint64_t) client_buff.buffer->addr;
    client_send_sge.length = (uint32_t) client_buff.buffer->length;
    client_send_sge.lkey = client_buff.buffer->lkey;

    bzero(&client_send_wr, sizeof(client_send_wr));
    client_send_wr.sg_list = &client_send_sge;
    client_send_wr.num_sge = 1;
    client_send_wr.opcode = IBV_WR_SEND;
    client_send_wr.send_flags = IBV_SEND_SIGNALED;

    HANDLE_NZ(ibv_post_send(client_res->qp,
                        &client_send_wr,
                        &bad_client_send_wr));

    info("Send request with OFFSET is successful \n");
    return 0;
}

static int disconnect_and_cleanup()
{
    int ret = -1;
    ret = rdma_disconnect(client_res->client_id);
    if (ret) {
        error("Failed to disconnect, errno: %d \n", -errno);
    }

    /* Destroy QP */
    rdma_destroy_qp(client_res->client_id);

    /* Destroy client cm id */
    ret = rdma_destroy_id(client_res->client_id);
    if (ret) {
        error("Failed to destroy client id cleanly, %d \n", -errno);
    }
    /* Destroy CQ */
    ret = ibv_destroy_cq(client_res->cq);
    if (ret) {
        error("Failed to destroy completion queue cleanly, %d \n", -errno);
    }
    /* Destroy completion channel */
    ret = ibv_destroy_comp_channel(client_res->completion_channel);
    if (ret) {
        error("Failed to destroy completion channel cleanly, %d \n", -errno);
    }

    rdma_buffer_deregister(server_buff.buffer);
    rdma_buffer_deregister(client_buff.buffer);

    /* Destroy protection domain */
    ret = ibv_dealloc_pd(client_res->pd);
    if (ret) {
        error("Failed to destroy client protection domain cleanly, %d \n", -errno);

    }
    rdma_destroy_event_channel(cm_event_channel);
    printf("Client resource clean up is complete \n");
    return 0;
}

static void poll_for_completion_events(int num_wc) {
    struct ibv_wc wc;
    int total_wc = process_work_completion_events(client_res->completion_channel, &wc, num_wc);

    for (int i = 0 ; i < total_wc; i++) {
        if( (&(wc) + i)->opcode & IBV_WC_RECV ) {
            if ( server_buff.message->type == ADDRESS ) {
                show_exchange_buffer(server_buff.message);
            }
        }
    }
    debug("done\n");
}

static void build_memory_map(struct per_connection_struct *conn) {
    conn->local_memory_region = malloc( DATA_SIZE  + (8 * (DATA_SIZE / BLOCK_SIZE)));
    memset(conn->local_memory_region, 2, DATA_SIZE  + (8 * (DATA_SIZE / BLOCK_SIZE)));

    conn->local_memory_region_mr = rdma_buffer_register(client_res->pd,
                                                  conn->local_memory_region,
                                                  DATA_SIZE  + (8 * (DATA_SIZE / BLOCK_SIZE)),
                                                  (IBV_ACCESS_LOCAL_WRITE|
                                                   IBV_ACCESS_REMOTE_READ|
                                                   IBV_ACCESS_REMOTE_WRITE));

    debug("Initiated local memory map: %p\n", conn->local_memory_region)
}

static void read_memory_map(struct per_connection_struct* conn) {
    memcpy(&conn->server_mr, &server_buff.message->data.mr, sizeof(conn->server_mr));

    client_send_sge.addr = (uintptr_t)(conn->local_memory_region);
    client_send_sge.length = (uint32_t) DATA_SIZE  + (8 * (DATA_SIZE / BLOCK_SIZE));
    client_send_sge.lkey = conn->local_memory_region_mr->lkey;

    bzero(&client_send_wr, sizeof(client_send_wr));
    client_send_wr.sg_list = &client_send_sge;
    client_send_wr.num_sge = 1;
    client_send_wr.opcode = IBV_WR_RDMA_READ;
    client_send_wr.send_flags = IBV_SEND_SIGNALED;
    client_send_wr.wr.rdma.remote_addr = (uintptr_t) conn->server_mr.addr;
    client_send_wr.wr.rdma.rkey = conn->server_mr.rkey;
    HANDLE_NZ(ibv_post_send(client_res->qp,
                            &client_send_wr,
                            &bad_client_send_wr));

    debug("RDMA read the remote memory map. \n");
}

const char* generateString() {
    char* _str = malloc(BLOCK_SIZE);
    char a = 'a';
    for( int i = 0; i < BLOCK_SIZE - 1; i++) {

        strncat(_str, &a, 1);
        if(a == 'z')
            a = 'a';
        else
            a = a + 1;
    }
    return _str;
}
static void write_to_memory_map(struct per_connection_struct* conn) {
    const char* str_to_copy = generateString();
    for ( int i = 0 ; i < (DATA_SIZE / BLOCK_SIZE); i++) {
        memcpy(conn->local_memory_region + (i * BLOCK_SIZE) + (8 * (DATA_SIZE / BLOCK_SIZE)), str_to_copy, BLOCK_SIZE );
    }
    client_send_sge.addr = (uintptr_t)(conn->local_memory_region);
    client_send_sge.length = (uint32_t) DATA_SIZE  + (8 * (DATA_SIZE / BLOCK_SIZE));
    client_send_sge.lkey = conn->local_memory_region_mr->lkey;
    /* now we link to the send work request */
    bzero(&client_send_wr, sizeof(client_send_wr));
    client_send_wr.sg_list = &client_send_sge;
    client_send_wr.num_sge = 1;
    client_send_wr.opcode = IBV_WR_RDMA_WRITE;
    client_send_wr.send_flags = IBV_SEND_SIGNALED;
    /* we have to tell server side info for RDMA */
    client_send_wr.wr.rdma.rkey = conn->server_mr.rkey;
    client_send_wr.wr.rdma.remote_addr = (uintptr_t) conn->server_mr.addr;
    /* Now we post it */
    HANDLE_NZ(ibv_post_send(client_res->qp,
                        &client_send_wr,
                        &bad_client_send_wr));
    debug("RDMA write the remote memory map. \n");
}


static void write_to_memory_map_in_offset(struct per_connection_struct* conn, int offset, const char* string_to_write) {
    strcpy(conn->local_memory_region + (offset*BLOCK_SIZE) + (8 * (DATA_SIZE / BLOCK_SIZE)), string_to_write);

    client_send_sge.addr = (uintptr_t)(conn->local_memory_region + (8 * (DATA_SIZE / BLOCK_SIZE)) + (offset * BLOCK_SIZE));
    client_send_sge.length = (uint32_t) BLOCK_SIZE;
    client_send_sge.lkey = conn->local_memory_region_mr->lkey;

    bzero(&client_send_wr, sizeof(client_send_wr));
    client_send_wr.sg_list = &client_send_sge;
    client_send_wr.num_sge = 1;
    client_send_wr.opcode = IBV_WR_RDMA_WRITE;
    client_send_wr.send_flags = IBV_SEND_SIGNALED;

    client_send_wr.wr.rdma.rkey = conn->server_mr.rkey;
    client_send_wr.wr.rdma.remote_addr = (uintptr_t) conn->server_mr.addr + (8 * (DATA_SIZE / BLOCK_SIZE)) + (offset * BLOCK_SIZE);

    HANDLE_NZ(ibv_post_send(client_res->qp,
                            &client_send_wr,
                            &bad_client_send_wr));

    debug("RDMA write the remote memory map. \n");
}

static void read_from_memory_map_in_offset(struct per_connection_struct* conn, int offset) {
    memcpy(&conn->server_mr, &server_buff.message->data.mr, sizeof(conn->server_mr));

    client_send_sge.addr = (uintptr_t)(conn->local_memory_region + (8 * (DATA_SIZE / BLOCK_SIZE)) + (offset * BLOCK_SIZE));
    client_send_sge.length = (uint32_t) BLOCK_SIZE;
    client_send_sge.lkey = conn->local_memory_region_mr->lkey;

    bzero(&client_send_wr, sizeof(client_send_wr));
    client_send_wr.sg_list = &client_send_sge;
    client_send_wr.num_sge = 1;
    client_send_wr.opcode = IBV_WR_RDMA_READ;
    client_send_wr.send_flags = IBV_SEND_SIGNALED;
    client_send_wr.wr.rdma.remote_addr = (uintptr_t) conn->server_mr.addr + (8 * (DATA_SIZE / BLOCK_SIZE)) + (offset * BLOCK_SIZE);
    client_send_wr.wr.rdma.rkey = conn->server_mr.rkey;

    HANDLE_NZ(ibv_post_send(client_res->qp,
                            &client_send_wr,
                            &bad_client_send_wr));

    debug("RDMA read the remote memory map \n");
}

void* read_from_redis(void* args) {
    struct per_connection_struct* conn = args;

    redisContext *context = redisConnect("127.0.0.1", 6379);
    if (!context) {
        fprintf(stderr, "Error:  Can't connect to Redis\n");
        pthread_exit((void*)0);
    }

    char offset[2] = {'0', '\0'};
    redisReply* reply;
    char* previousValue = NULL;

    while (1) {
        reply = redisCommand(context, "GET %s", offset);

        if (reply == NULL || reply->type != REDIS_REPLY_STRING) {
            fprintf(stderr, "Error getting key value or key not found\n");
            freeReplyObject(reply);
            continue;
        }

        if (previousValue == NULL || strcmp(previousValue, reply->str) != 0) {
            info("Previous String (%s): %s\n", offset, previousValue);
            info("Updating %s to new string %s\n", previousValue, reply->str);

            write_to_memory_map_in_offset(conn, atoi(offset), reply->str);
            poll_for_completion_events(1);

            previousValue = strdup(reply->str);
            info("MAP_UPDATE: key: %s value: %s\n", offset, reply->str);
            print_memory_map(conn->local_memory_region);
        }

        freeReplyObject(reply);
    }
}

void* write_to_redis(void *args) {
    struct per_connection_struct *conn = args;

    redisContext *context = redisConnect("127.0.0.1", 6379);
    if (!context) {
        fprintf(stderr, "Error:  Can't connect to Redis\n");
        pthread_exit((void *) 0);
    }

    char *previousValue = NULL;
    char offset[2] = {'1', '\0'};
    redisReply *reply;

    while (1) {
        read_from_memory_map_in_offset(conn, atoi(offset));
        poll_for_completion_events(1);

        char *str = conn->local_memory_region + (8 * (DATA_SIZE / BLOCK_SIZE)) + (atoi(offset) * BLOCK_SIZE);

        if (previousValue == NULL || strcmp(previousValue, str) != 0) {
            info("Previous String (%s): %s\n", offset, previousValue);
            info("Updating %s to new string %s\n", previousValue, str);

            reply = redisCommand(context, "SET %s %s", offset, str);
            if (!reply || context->err) {
                fprintf(stderr, "Error:  Can't send command to Redis\n");
                pthread_exit((void *) 0);
            }
            info("REDIS_UPDATE: key: %s value: %s => %s \n", offset, str, reply->str);
            previousValue = strdup(str);
            print_memory_map(conn->local_memory_region);
        }
        sleep(1);
    }
}

static int wait_for_event(struct sockaddr_in *s_addr) {

    struct rdma_cm_event *dummy_event = NULL;
    struct per_connection_struct* connection = NULL;

    pthread_t thread1, thread2;
    while(rdma_get_cm_event(cm_event_channel, &dummy_event) == 0) {
        struct rdma_cm_event cm_event;
        memcpy(&cm_event, dummy_event, sizeof(*dummy_event));
        info("%s event received \n", rdma_event_str(cm_event.event));
        switch(cm_event.event) {
            case RDMA_CM_EVENT_ADDR_RESOLVED:
                HANDLE_NZ(rdma_ack_cm_event(dummy_event));
                connection = (struct per_connection_struct*) malloc (sizeof(struct per_connection_struct*));
                rdma_resolve_route(client_res->client_id, 2000);
                break;
            case RDMA_CM_EVENT_ROUTE_RESOLVED:
                HANDLE_NZ(rdma_ack_cm_event(dummy_event));
                setup_client_resources(s_addr);
                build_memory_map(connection);
                post_recv_server_memory_map();
                connect_to_server();
                break;
            case RDMA_CM_EVENT_ESTABLISHED:
                HANDLE_NZ(rdma_ack_cm_event(dummy_event));
                post_send_to_server();
                poll_for_completion_events(2); // post_recv_server_memory_map, post_send_to_server
                read_memory_map(connection);
                poll_for_completion_events(1);
                pthread_create(&thread1, NULL, read_from_redis, (void*) connection);
                pthread_create(&thread2, NULL, write_to_redis, (void *) connection);
                break;
            case RDMA_CM_EVENT_DISCONNECTED:
                HANDLE_NZ(rdma_ack_cm_event(dummy_event));
                disconnect_and_cleanup();
                break;
            default:
                error("Event not found %s", (char *) cm_event.event);
                break;
        }
    }
}

int main(int argc, char **argv) {
    struct sockaddr_in server_sockaddr;
    int ret, option;

    bzero(&server_sockaddr, sizeof server_sockaddr);
    server_sockaddr.sin_family = AF_INET;
    server_sockaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    while ((option = getopt(argc, argv, "s:a:p:")) != -1) {
        switch (option) {
            case 'a':
                /* remember, this overwrites the port info */
                ret = get_addr(optarg, (struct sockaddr*) &server_sockaddr);
                if (ret) {
                    error("Invalid IP \n");
                    return ret;
                }
                break;
            case 'p':
                /* passed port to listen on */
                server_sockaddr.sin_port = htons(strtol(optarg, NULL, 0));
                break;
        }
    }
    if (!server_sockaddr.sin_port) {
        server_sockaddr.sin_port = htons(DEFAULT_RDMA_PORT);
    }
    client_prepare_connection(&server_sockaddr);
    wait_for_event(&server_sockaddr);
    return ret;

}