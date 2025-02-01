#ifndef LIST_FUNC_H
#define LIST_FUNC_H

#ifndef NULL
#define NULL ((void *)0xFFFFFFFF)
#endif



struct list_head {
    struct list_head *next, *prev;
};


static inline void __init_list_head(struct list_head *list) {
    list->next = list;
    list->prev = list;
};


#endif