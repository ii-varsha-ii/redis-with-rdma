#include "server.h"


#define DEFAULT_RDMA_PORT (12345)

static struct rdma_event_channel *cm_event_channel = NULL;
static struct rdma_cm_id *cm_server_id = NULL, *cm_client_id = NULL;
static struct ibv_pd *pd = NULL;
static struct ibv_comp_channel *io_completion_channel = NULL;
static struct ibv_cq *cq = NULL;
static struct ibv_qp_init_attr qp_init_attr;
static struct ibv_qp *client_qp = NULL;

int process_rdma_cm_event(struct rdma_event_channel *event_channel,
                          enum rdma_cm_event_type expected_event,
                          struct rdma_cm_event **cm_event)
{
    int ret = 1;

    // get the received event
    ret = rdma_get_cm_event(event_channel, cm_event);
    if (ret) {
        error("Failed to retrieve a cm event, errno: %d \n",
                   -errno);
        return -errno;
    }

    if(0 != (*cm_event)->status){
        error("CM event has non zero status: %d\n", (*cm_event)->status);
        ret = -((*cm_event)->status);

        // ack the event
        rdma_ack_cm_event(*cm_event);
        return ret;
    }

    // if the event is not the expected event
    if ((*cm_event)->event != expected_event) {
        error("Unexpected event received: %s [ expecting: %s ]",
                   rdma_event_str((*cm_event)->event),
                   rdma_event_str(expected_event));
        // ack the event
        rdma_ack_cm_event(*cm_event);
        return -1;
    }
    info("A new %s type event is received \n", rdma_event_str((*cm_event)->event));
    return ret;
}

static int setup_client_resources()
{
    int ret = -1;
    if(!cm_client_id){
        error("Client id is still NULL \n");
        return -EINVAL;
    }
    /* We have a valid connection identifier, lets start to allocate
     * resources. We need:
     * 1. Protection Domains (PD)
     * 2. Memory Buffers
     * 3. Completion Queues (CQ)
     * 4. Queue Pair (QP)
     * Protection Domain (PD) is similar to a "process abstraction"
     * in the operating system. All resources are tied to a particular PD.
     * And accessing recourses across PD will result in a protection fault.
     */
    pd = ibv_alloc_pd(cm_client_id->verbs
            /* verbs defines a verb's provider,
             * i.e an RDMA device where the incoming
             * client connection came */);
    if (!pd) {
        error("Failed to allocate a protection domain errno: %d\n",
                   -errno);
        return -errno;
    }

    info("A new protection domain is allocated at %p \n", pd);
    /* Now we need a completion channel, were the I/O completion
     * notifications are sent. Remember, this is different from connection
     * management (CM) event notifications.
     * A completion channel is also tied to an RDMA device, hence we will
     * use cm_client_id->verbs.
     */
    io_completion_channel = ibv_create_comp_channel(cm_client_id->verbs);
    if (!io_completion_channel) {
        error("Failed to create an I/O completion event channel, %d\n",
                   -errno);
        return -errno;
    }
    info("An I/O completion event channel is created at %p \n",
          io_completion_channel);
    /* Now we create a completion queue (CQ) where actual I/O
     * completion metadata is placed. The metadata is packed into a structure
     * called struct ibv_wc (wc = work completion). ibv_wc has detailed
     * information about the work completion. An I/O request in RDMA world
     * is called "work" ;)
     */
#define CQ_CAPACITY (16)
/* MAX SGE capacity */
#define MAX_SGE (2)
/* MAX work requests */
#define MAX_WR (8)

    cq = ibv_create_cq(cm_client_id->verbs /* which device*/,
                       CQ_CAPACITY /* maximum capacity*/,
                       NULL /* user context, not used here */,
                       io_completion_channel /* which IO completion channel */,
                       0 /* signaling vector, not used here*/);
    if (!cq) {
        error("Failed to create a completion queue (cq), errno: %d\n",
                   -errno);
        return -errno;
    }
    info("Completion queue (CQ) is created at %p with %d elements \n",
          cq, cq->cqe);
    /* Ask for the event for all activities in the completion queue*/
    ret = ibv_req_notify_cq(cq /* on which CQ */,
                            0 /* 0 = all event type, no filter*/);
    if (ret) {
        error("Failed to request notifications on CQ errno: %d \n",
                   -errno);
        return -errno;
    }
    /* Now the last step, set up the queue pair (send, recv) queues and their capacity.
     * The capacity here is define statically but this can be probed from the
     * device. We just use a small number as defined in rdma_common.h */
    bzero(&qp_init_attr, sizeof qp_init_attr);
    qp_init_attr.cap.max_recv_sge = MAX_SGE; /* Maximum SGE per receive posting */
    qp_init_attr.cap.max_recv_wr = MAX_WR; /* Maximum receive posting capacity */
    qp_init_attr.cap.max_send_sge = MAX_SGE; /* Maximum SGE per send posting */
    qp_init_attr.cap.max_send_wr = MAX_WR; /* Maximum send posting capacity */
    qp_init_attr.qp_type = IBV_QPT_RC; /* QP type, RC = Reliable connection */
    /* We use same completion queue, but one can use different queues */
    qp_init_attr.recv_cq = cq; /* Where should I notify for receive completion operations */
    qp_init_attr.send_cq = cq; /* Where should I notify for send completion operations */
    /*Lets create a QP */
    ret = rdma_create_qp(cm_client_id /* which connection id */,
                         pd /* which protection domain*/,
                         &qp_init_attr /* Initial attributes */);
    if (ret) {
        error("Failed to create QP due to errno: %d\n", -errno);
        return -errno;
    }
    /* Save the reference for handy typing but is not required */
    client_qp = cm_client_id->qp;
    info("Client QP created at %p\n", client_qp);
    return ret;
}


static int start_rdma_server(struct sockaddr_in *server_socket_addr) {
    struct rdma_cm_event *cm_event = NULL;

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

    // when a client connects, it sends a connect request
    ret = process_rdma_cm_event(cm_event_channel,
                                RDMA_CM_EVENT_CONNECT_REQUEST,
                                &cm_event);
    if (ret) {
        error("Failed to get cm event, ret = %d \n" , ret);
        return ret;
    }

    cm_client_id = cm_event->id;

    // ack connect request
    ret = rdma_ack_cm_event(cm_event);
    if (ret) {
        error("Failed to acknowledge the cm event errno: %d \n", -errno);
        return -errno;
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
}