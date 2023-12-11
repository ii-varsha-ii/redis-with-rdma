#include "utils.h"


void print_memory_map(const char* memory_region) {
    info("-------------------\n")
    info("Memory Map\n")
    info("-------------------\n")
    for(int i = 0; i < (DATA_SIZE / BLOCK_SIZE); i++) {
        info("%d:%s \n", i, memory_region + (i * BLOCK_SIZE) + (8 * (DATA_SIZE / BLOCK_SIZE)));
    }
    info("-------------------\n")
}

void show_exchange_buffer(struct msg *attr) {
    info("---------------------------------------------------------\n");
    info("message %p\n", attr);
    info("message, type: %d\n", attr->type);
    if(attr->type == OFFSET) {
        info("message: offset: %lu \n", attr->data.offset);
    }
    if (attr->type == ADDRESS){
        info("message: data.mr.address: %p \n", attr->data.mr.addr);
    }
    info("---------------------------------------------------------\n");
}

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
    printf("%p\n", cq_ptr);
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

    debug("%d WC are completed \n", total_wc)
    /* Now we check validity and status of I/O work completions */
    for( i = 0 ; i < total_wc ; i++) {
        if (wc[i].status != IBV_WC_SUCCESS) {
            error("Work completion (WC) has error status: %s at index %d \n",
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
