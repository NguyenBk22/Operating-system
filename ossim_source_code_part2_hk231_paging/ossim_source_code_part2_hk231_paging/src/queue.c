#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t * q) {
        if (q == NULL) return 1;
	return (q->size == 0);
}

void enqueue(struct queue_t * q, struct pcb_t * proc) {
        /* TODO: put a new process to queue [q] */
	if (q == NULL || proc == NULL) {
        // Check for invalid queue or process
        return;
    }
    
    if (q->size == MAX_QUEUE_SIZE) {
        // Check if the queue is already full
        return;
    }

    // Add the process to the queue based on its priority
    int i;
    for (i = q->size - 1; i >= 0; i--) {
        if (proc->priority > q->proc[i]->priority) {
            q->proc[i + 1] = q->proc[i];
        } else {
            break;
        }
    }
    q->proc[i + 1] = proc;
    q->size++;
}

struct pcb_t * dequeue(struct queue_t * q) {
        /* TODO: return a pcb whose prioprity is the highest
         * in the queue [q] and remember to remove it from q
         * */
    struct pcb_t *proc = NULL;
    if (empty(q))
    {
	// If the queue is empty, return NULL
        proc = NULL;
    }
    else
    {
        int max = 0;
        uint32_t priority_max = q->proc[0]->priority;
        for (int i = 0; i < q->size; i++)
        {
            if (q->proc[i]->priority > priority_max)
            {
                priority_max = q->proc[i]->priority;
                max = i;
            }
        }
        proc = q->proc[max];
        for (int i = max; i < q->size - 1; i++)
        {
            q->proc[i] = q->proc[i + 1];
        }
        q->size--;
    }
    return proc;
}

