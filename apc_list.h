#ifndef APC_LIST_H
#define APC_LIST_H

typedef struct _apc_list_element {
	struct _apc_list_element *next;
	struct _apc_list_element *prev;
	void *data;
} apc_list_element;
	

typedef struct _apc_list {
	apc_list_element *head;
	apc_list_element *tail;
	int (*list_ctor)(size_t);
	void (*list_dtor)(void *);
} apc_list;

extern void apc_list_create(apc_list **list, int (*apc_list_ctor)(size_t), void (*apc_list_dtor)(void *));
extern void apc_list_prepend(apc_list *list, void *data);
extern void apc_list_append(apc_list *list, void *data);
extern void apc_list_apply(apc_list *list, void (*apply_func)(void *));
extern void apc_list_destroy(apc_list *list);
extern void apc_list_clean(apc_list *list);

#endif
