#include "utils.h"
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

void show_rdma_cmid(struct rdma_cm_id *id)
{
    if(!id){
        error("Passed ptr is NULL\n");
        return;
    }
    printf("RDMA cm id at %p \n", id);
    if(id->verbs && id->verbs->device)
        printf("dev_ctx: %p (device name: %s) \n", id->verbs,
               id->verbs->device->name);
    if(id->channel)
        printf("cm event channel %p\n", id->channel);
    printf("QP: %p, port_space %x, port_num %u \n", id->qp,
           id->ps,
           id->port_num);
}

void show_rdma_buffer_attr(struct rdma_buffer_attr *attr){
    if(!attr){
        error("Passed attr is NULL\n");
        return;
    }
    printf("---------------------------------------------------------\n");
    printf("buffer attr, addr: %p , len: %u , stag : 0x%x \n",
           (void*) attr->address,
           (unsigned int) attr->length,
           attr->stag.local_stag);
    printf("---------------------------------------------------------\n");
}

// Allocate a memory region - calloc and register that memory region
struct ibv_mr* rdma_buffer_alloc(struct ibv_pd *pd, uint32_t size,
                                 enum ibv_access_flags permission)
{
    struct ibv_mr *mr = NULL;
    if (!pd) {
        error("Protection domain is NULL \n");
        return NULL;
    }
    void *buf = calloc(1, size);
    if (!buf) {
        error("failed to allocate buffer, -ENOMEM\n");
        return NULL;
    }
    info("Buffer allocated: %p , len: %u \n", buf, size);
    mr = rdma_buffer_register(pd, buf, size, permission);
    if(!mr){
        free(buf);
    }
    return mr;
}

// Register a memory region - ibv_reg_mr
struct ibv_mr *rdma_buffer_register(struct ibv_pd *pd,
                                    void *addr, uint32_t length,
                                    enum ibv_access_flags permission)
{
    struct ibv_mr *mr = NULL;
    if (!pd) {
        error("Protection domain is NULL, ignoring \n");
        return NULL;
    }
    mr = ibv_reg_mr(pd, addr, length, permission);
    if (!mr) {
        error("Failed to create mr on buffer, errno: %d \n", -errno);
        return NULL;
    }
    info("Registered: %p , len: %u , stag: 0x%x \n",
          mr->addr,
          (unsigned int) mr->length,
          mr->lkey);
    return mr;
}

// Free memory and Deregister that memory region -
void rdma_buffer_free(struct ibv_mr *mr)
{
    if (!mr) {
        error("Passed memory region is NULL, ignoring\n");
        return ;
    }
    void *to_free = mr->addr;
    rdma_buffer_deregister(mr);
    info("Buffer %p free'ed\n", to_free);
    free(to_free);
}

// Deregister that memory region - ibv_dereg_mr
void rdma_buffer_deregister(struct ibv_mr *mr)
{
    if (!mr) {
        error("Passed memory region is NULL, ignoring\n");
        return;
    }
    info("Deregistered: %p , len: %u , stag : 0x%x \n",
          mr->addr,
          (unsigned int) mr->length,
          mr->lkey);
    ibv_dereg_mr(mr);
}


int process_work_completion_events (struct ibv_comp_channel *comp_channel,
                                    struct ibv_wc *wc, int max_wc)
{
    struct ibv_cq *cq_ptr = NULL;
    void *context = NULL;
    int ret = -1, i, total_wc = 0;
    /* We wait for the notification on the CQ channel */
    ret = ibv_get_cq_event(comp_channel, /* IO channel where we are expecting the notification */
                           &cq_ptr, /* which CQ has an activity. This should be the same as CQ we created before */
                           &context); /* Associated CQ user context, which we did set */
    if (ret) {
        error("Failed to get next CQ event due to %d \n", -errno);
        return -errno;
    }

    /* Request for more notifications. */
    ret = ibv_req_notify_cq(cq_ptr, 0);
    if (ret){
        error("Failed to request further notifications %d \n", -errno);
        return -errno;
    }
    /* We got notification. We reap the work completion (WC) element. It is
 * unlikely but a good practice it write the CQ polling code that
    * can handle zero WCs. ibv_poll_cq can return zero. Same logic as
    * MUTEX conditional variables in pthread programming.
 */
    // poll through the cq and get all the notifications
    total_wc = 0;
    do {
        ret = ibv_poll_cq(cq_ptr /* the CQ, we got notification for */,
                          max_wc - total_wc /* number of remaining WC elements*/,
                          wc + total_wc/* where to store */);
        if (ret < 0) {
            error("Failed to poll cq for wc due to %d \n", ret);
            /* ret is errno here */
            return ret;
        }
        total_wc += ret;
    } while (total_wc < max_wc);
    info("%d WC are completed \n", total_wc);
    /* Now we check validity and status of I/O work completions */
    for( i = 0 ; i < total_wc ; i++) {
        if (wc[i].status != IBV_WC_SUCCESS) {
            error("Work completion (WC) has error status: %s at index %d",
                       ibv_wc_status_str(wc[i].status), i);
            /* return negative value */
            return -(wc[i].status);
        }
    }
    /* Similar to connection management events, we need to acknowledge CQ events */
    ibv_ack_cq_events(cq_ptr,
                      1 /* we received one event notification */
        );
    return total_wc;
}


/* Code acknowledgment: rping.c from librdmacm/examples */
int get_addr(char *dst, struct sockaddr *addr)
{
    struct addrinfo *res;
    int ret = -1;
    ret = getaddrinfo(dst, NULL, NULL, &res);
    if (ret) {
        error("getaddrinfo failed - invalid hostname or IP address\n");
        return ret;
    }
    memcpy(addr, res->ai_addr, sizeof(struct sockaddr_in));
    freeaddrinfo(res);
    return ret;
}