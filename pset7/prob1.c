/*
 * prob1.c
 *
 *  Created on:
 *      Author:
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the include file for the SQLite library */
/* do not use sqlite.h or sqlite3ext.h */
#include <sqlite3.h>
#include "prob1.h"

/* static variables for the root of the tree and number of records */
p_tnode ptreeroot = NULL;
unsigned int record_count = 0;

/* allocate_record() - allocate a new record on the heap */
struct s_record * allocate_record()
{
	struct s_record * precord = (struct s_record *) malloc(sizeof(struct s_record));
	precord->irecord = 0;
	precord->name = NULL; /* movie name used as key */
	precord->category = NULL;
	precord->year = 0;
	precord->format = NULL;
	precord->language = NULL;
	precord->url = NULL;
	return precord;
}

/* free_record() - frees the record structure and associated strings */
void free_record(struct s_record * precord)
{
	if (precord->name) free(precord->name);
	if (precord->category) free(precord->category);
	if (precord->format) free(precord->format);
	if (precord->language) free(precord->language);
	if (precord->url) free(precord->url);
	free(precord);
}

/* display_record -- output record information to file pointer fp */
void display_record(struct s_record * precord, FILE * fp)
{
	fprintf(fp, "[#%d] %s (%d): %s, %s, %s, <%s>\n", precord->irecord, precord->name, precord->year, precord->category, precord->format, precord->language, precord->url);
}

/* allocate_b_tree_node() - creates a new B-tree node on the heap */
p_tnode allocate_b_tree_node(void)
{
	p_tnode pnode = (p_tnode) malloc(sizeof(struct s_tnode));
	pnode->nkeys = 0;
	for (int n = 0; n < 2 * T; n++)
		pnode->children[n] = NULL;
	pnode->parent = NULL;
	return pnode;
}

/* free_tnode() - frees a node in the B-tree,
 * its associated record, and all its children from memory
 */
void free_tnode(p_tnode pnode)
{
	unsigned int n;
	for (n = 0; n < 2 * T; n++) {
		if (pnode->children[n] != NULL)
			free_tnode(pnode->children[n]);
	}

	for (n = 0; n < pnode->nkeys; n++) {
		if (pnode->values[n])
			free_record(pnode->values[n]);
	}

	free(pnode);
}

/* function for comparing two keys; simply calls the case-insensitive
 * string comparison function strcasecmp() declared in string.h.
 * Returns zero if both equal, positive if key1 > key2, negative if key1 < key2
 */
int key_compare(const nodekey key1, const nodekey key2)
{
	return strcasecmp(key1, key2);
}

/* find_index() - performs binary search of list of keys in node
 * to find either the index of the existing key, or the insertion
 * point of a new key. Returns nonnegative index of insertion point
 * if new key, or -(index+1) for existing key found at index
 *
 */
int find_index(nodekey key, p_tnode pnode)
{
	/* find in between */
	int comparison, L = 0, R = pnode->nkeys - 1, M;
	// int ibetween = 0; /* index to return */

	while (L <= R) {
		M = (L + R) / 2;

		comparison = key_compare(key, pnode->values[M]->name);

		if (comparison == 0) {
			return - (M + 1);
		}

		if (comparison > 0) {
			L = M + 1;
		} else {
			R = M - 1;
		}
	}

	return L;
}

/* split_node() - splits a full node in the B-tree into two separate nodes,
 * possibly creating a new root node in the process
 * should be no need to modify this function
 */
void split_node(p_tnode * ppnode, int * poffset)
{
	p_tnode pnode = *ppnode;
	int median = pnode->nkeys>>1;
	p_tnode parent = pnode->parent, split = allocate_b_tree_node();

	if (poffset != NULL) {
		/* update offset index and ppnode to point to new insertion point */
		if (*poffset > median) {
			/* move to new (split) node */
			*poffset -= (median+1);
			*ppnode = split;
		}
	}
	if (parent) {
		unsigned int insert = find_index(pnode->keys[median],parent);

		/* move median into parent */
		for (unsigned int i = parent->nkeys; i > insert; i--) {
			parent->keys[i] = parent->keys[i-1];
			parent->values[i] = parent->values[i-1];
			parent->children[i+1] = parent->children[i];
		}
		parent->keys[insert] = pnode->keys[median];
		parent->values[insert] = pnode->values[median];
		parent->children[insert] = pnode;
		parent->nkeys++;

		/* move half from pnode into new node */
		for (unsigned int i = median + 1; i < pnode->nkeys; i++) {
			split->keys[i-(median+1)] = pnode->keys[i];
			split->values[i-(median+1)] = pnode->values[i];
		}
		for (unsigned int i = median + 1; i < pnode->nkeys+1; i++) {
			split->children[i-(median+1)] = pnode->children[i];
			if (pnode->children[i] != NULL)
				pnode->children[i]->parent = split;
			pnode->children[i] = NULL;
		}
		split->nkeys = pnode->nkeys - (median+1);
		pnode->nkeys = median;
		parent->children[insert+1] = split;
		split->parent = parent;
	} else {
		/* split root */
		parent = ptreeroot = allocate_b_tree_node();
		parent->keys[0] = pnode->keys[median];
		parent->values[0] = pnode->values[median];
		parent->children[0] = pnode;
		parent->nkeys = 1;
		pnode->parent = parent;

		/* move half from pnode into new node */
		for (unsigned int i = median + 1; i < pnode->nkeys; i++) {
			split->keys[i-(median+1)] = pnode->keys[i];
			split->values[i-(median+1)] = pnode->values[i];
		}
		for (unsigned int i = median + 1; i < pnode->nkeys+1; i++) {
			split->children[i-(median+1)] = pnode->children[i];
			if (pnode->children[i] != NULL)
				pnode->children[i]->parent = split;
			pnode->children[i] = NULL;
		}
		split->nkeys = pnode->nkeys - (median + 1);
		pnode->nkeys = median;
		parent->children[1] = split;
		split->parent = parent;
	}
}

/* add_element() - add the record with the specified key to the B-tree.
 * returns a pointer to an already existing record with the same key,
 * in which case, the new record was not inserted, or NULL on success
 * should be no need to modify this function
 */
nodevalue * add_element(nodekey key, nodevalue * pvalue)
{
	/* find leaf */
	p_tnode pleaf = ptreeroot, parent = NULL;
	int ichild, i;

	while (pleaf != NULL) {
		ichild = find_index(key, pleaf);
		if (ichild < 0) /* already exists */
			return pleaf->values[-(ichild+1)];
		if (pleaf->nkeys == 2*T-1) {
			/* split leaf into two nodes */
			split_node(&pleaf, &ichild);
		}
		parent = pleaf;
		pleaf = pleaf->children[ichild];
	}

	pleaf = parent;
	record_count++; /* record actually added to tree */

	/* enough room, just add to leaf */
	for (i = pleaf->nkeys; i > ichild; i--) {
		pleaf->keys[i] = pleaf->keys[i-1];
		pleaf->values[i] = pleaf->values[i-1];
	}
	pleaf->keys[ichild] = key;
	pleaf->values[ichild] = pvalue;
	pleaf->nkeys++;

	return NULL;
}

/* inorder_traversal() - traverse B-tree, in sorted order
 * similar to binary search tree traversal (except nodes have multiple values and children)
 * writes record info to file pointer fp
 *
 */
void inorder_traversal(p_tnode pnode, FILE * fp)
{
	unsigned int n;

	for (n = 0; n < pnode->nkeys; n++) {
		if (pnode->children[n] != NULL) {
			inorder_traversal(pnode->children[n], fp);
		}
		display_record(pnode->values[n], fp);
	}
	if (pnode->children[n] != NULL) {
		inorder_traversal(pnode->children[n], fp);
	}
}

/* find_value() - locate value for specified key in B-tree
 * need to return pointer to record structure, or NULL if record not found
 */
nodevalue * find_value(const nodekey key)
{
	int ichild;
	p_tnode pleaf = ptreeroot; /* start at root */

	while (pleaf != NULL) {
		ichild = find_index(key, pleaf);

		if (ichild < 0) {
			return pleaf->values[-(ichild + 1)];
		}

		pleaf = pleaf->children[ichild];
	}

	return NULL; /* didn't find it */
}

/* allocate_b_tree_nodesult() - print information from the database
 * this function is a valid callback for use with sqlite3_exec()
 * pextra is unused
 *
 * needs to return zero (nonzero aborts SQL query)
 */
int display_result(void * pextra, int nfields, char ** arrvalues, char ** arrfieldnames)
{
	char * string = (char *) pextra;

	if (string != NULL) {
		puts(string);
	}

	for (int n = 0; n < nfields; n++) {
		printf("%s: [%s]\n", arrfieldnames[n], arrvalues[n]);
	}

	puts("");
	return 0;
}

/* store_result() - store record from database in B-tree
 * this function is also a valid callback for use with sqlite3_exec()
 * pextra is again unused
 *
 * needs to return zero (nonzero aborts SQL query)
 */
int store_result(void * pextra, int nfields, char ** arrvalues, char ** arrfieldnames)
{
	int n;

	/* allocate record on heap */
	struct s_record * prec = allocate_record();

	prec->irecord = record_count + 1;
	for (n = 0; n < nfields; n++) {
		if (strcasecmp(arrfieldnames[n], "MovieTitle") == 0)
			prec->name = strdup(arrvalues[n]); /* key */
		else if (strcasecmp(arrfieldnames[n], "MovieCategory") == 0)
			prec->category = strdup(arrvalues[n]);
		else if (strcasecmp(arrfieldnames[n], "ProductionYear") == 0)
			prec->year = atoi(arrvalues[n]);
		else if (strcasecmp(arrfieldnames[n], "Format") == 0)
			prec->format = strdup(arrvalues[n]);
		else if (strcasecmp(arrfieldnames[n], "Language") == 0)
			prec->language = strdup(arrvalues[n]);
		else if (strcasecmp(arrfieldnames[n], "Web") == 0)
			prec->url = strdup(arrvalues[n]);
	}

	/* add record to B-tree */
	if (add_element(prec->name, prec) != NULL) {
		/* element already exists -- don't add record */
		printf("Duplicate record exists: "); /* diagnostic value only */
		display_record(prec, stdout);
		free_record(prec);
	}

	return 0;
}

int initialize_db(const char * filename)
{
  const char sql[] = "SELECT * FROM movies";
  sqlite3 * database = NULL;

  if (sqlite3_open(filename, &database)){
		return 1;
	}

  char * error_message = NULL;

  ptreeroot = allocate_b_tree_node();
	sqlite3_exec(database, sql, store_result, NULL, &error_message);

  if (error_message) {
    return 2;
  }

  sqlite3_close(database);

  return 0;
}

int locate_movie(const char * title)
{
  nodevalue * found_movie = NULL;

  found_movie = find_value((nodekey) title);

  if (found_movie) {
    display_record(found_movie, stdout);
    return 1;
  }

  puts("Movie not in database");

  return 0;
}

void dump_sorted_list(const char * filename)
{
  FILE * file = fopen(filename, "w");
  inorder_traversal(ptreeroot, file);
	fclose(file);
}

void cleanup(void)
{
  free_tnode(ptreeroot);
}
