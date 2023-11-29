#include "server.h"


#define DEFAULT_RDMA_PORT (12345)

static struct rdma_event_channel *cm_event_channel = NULL;
static struct rdma_cm_id *cm_server_id = NULL, *cm_client_id = NULL;

int process_rdma_cm_event(struct rdma_event_channel *event_channel,
                          enum rdma_cm_event_type expected_event,
                          struct rdma_cm_event **cm_event)
{
    int ret = 1;

    // get the received event
    ret = rdma_get_cm_event(event_channel, cm_event);
    if (ret) {
        rdma_error("Failed to retrieve a cm event, errno: %d \n",
                   -errno);
        return -errno;
    }

    if(0 != (*cm_event)->status){
        rdma_error("CM event has non zero status: %d\n", (*cm_event)->status);
        ret = -((*cm_event)->status);

        // ack the event
        rdma_ack_cm_event(*cm_event);
        return ret;
    }

    // if the event is not the expected event
    if ((*cm_event)->event != expected_event) {
        rdma_error("Unexpected event received: %s [ expecting: %s ]",
                   rdma_event_str((*cm_event)->event),
                   rdma_event_str(expected_event));
        // ack the event
        rdma_ack_cm_event(*cm_event);
        return -1;
    }
    rdma_error("A new %s type event is received \n", rdma_event_str((*cm_event)->event));
    return ret;
}

static int start_rdma_server(struct sockaddr_in *server_socket_addr) {
    struct rdma_cm_event *cm_event = NULL;

    int ret;
    // create an event channel
    cm_event_channel = rdma_create_event_channel();
    if (!cm_event_channel) {
        rdma_error("[rdma]: Creating cm event channel failed with errno : (%d)", -errno);
        return -errno;
    }

    // create server id
    ret = rdma_create_id(cm_event_channel, &cm_server_id, NULL, RDMA_PS_TCP);
    if (ret) {
        rdma_error("Creating server cm id failed with errno: %d ", -errno);
        return -errno;
    }

    // bind server id to the given socket credentials
    ret = rdma_bind_addr(cm_server_id, (struct sockaddr*) server_socket_addr);
    if (ret) {
        rdma_error("Failed to bind server address, errno: %d \n", -errno);
        return -errno;
    }

    // listen
    ret = rdma_listen(cm_server_id, 8); /* backlog = 8 clients, same as TCP, see man listen*/
    if (ret) {
        rdma_error("rdma_listen failed to listen on server address, errno: %d ",
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
        rdma_error("Failed to get cm event, ret = %d \n" , ret);
        return ret;
    }

    cm_client_id = cm_event->id;

    // ack connect request
    ret = rdma_ack_cm_event(cm_event);
    if (ret) {
        rdma_error("Failed to acknowledge the cm event errno: %d \n", -errno);
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
                    rdma_error("Invalid IP");
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
        rdma_error("RDMA server failed to start cleanly, ret = %d \n", ret);
        return ret;
    }
}