#include "kdtree.h"

/* Friend of Friend:
 *
 * Connected component via edge enumeration.
 *
 * The connected components are stored as trees.
 *
 * (visit) two vertices i, j connected by an edge.
 *         if splay(i) differ from splay(j), the components shall be
 *         visited, call merge(i, j)
 *
 * (merge) Join two trees if they are connected by adding the root
 *         of i as a child of root of j.
 *
 * (splay) Move a leaf node to the direct child of the tree root
 *         and returns the root.
 *
 * One can show this algorithm ensures splay(i) is an label of
 * max connected components in the graph.
 *
 * Suitable for application where finding edges of a vertice is more expensive
 * than enumerating over edges.
 *
 * In KD-Tree implementation we apply optimization for overdense regions
 *
 * Before FOF we connect all nodes that are less than linking length in size (internally connected)
 * 
 * (connect) connect all points in a node into a tree, the first particle is the root.
 * 
 * In FOF, we use the dual tree algorithm for edge enumeration, but if two nodes are minimaly sperated
 * by linking length and both are internally connected
 *
 *   (nodemerge) merge(node1->first, node2->first)
 * 
 * For typically low resolution cosmological simulations this optimization improves speed by a few percent.
 * The improvement increases for highly clustered data.
 *
 * The storage is O(N) for the output labels + O(M) for the connection status of kdtree nodes.
 *
 * */

typedef struct TraverseData {
    ptrdiff_t * head;
    ptrdiff_t * ind; /* tree->ind */
    char * node_connected;
    double ll;
    double ll2;

    /* performance counters */
    ptrdiff_t visited;
    ptrdiff_t connected;
    ptrdiff_t skipped;
    ptrdiff_t maxdepth;
    ptrdiff_t nsplay;
    ptrdiff_t totaldepth;
} TraverseData;

typedef struct{
    TraverseData * trav;
    int self_connected;
    ptrdiff_t visited;
} VisitEdgeData;

static ptrdiff_t splay(TraverseData * d, ptrdiff_t i)
{
    ptrdiff_t depth = 0;
    ptrdiff_t r = i;
    /* First find the root */
    while(d->head[r] != r) {
        depth ++;
        r = d->head[r];
    }

#ifdef KD_FOF_UNSAFE
    /* link the nodes directly to the root to keep the tree flat */
    d->head[i] = r;
#else
    /* safe guard */
    while(d->head[i] != i) {
        ptrdiff_t t = d->head[i];
        d->head[i] = r;
        i = t;
    }
#endif

    /* update performance counters */
    if(depth > d->maxdepth) {
        d->maxdepth = depth;
    }
    d->totaldepth += depth;
    d->nsplay ++;

    return r;
}

static int
_kd_fof_visit_edge(void * data, KDEnumPair * pair);
static int
_kd_fof_visit_edge_first(void * data, KDEnumPair * pair);

static int
_kd_fof_check_nodes(void * data, KDEnumNodePair * pair)
{
    TraverseData * trav = (TraverseData*) data;

    if(trav->node_connected[pair->nodes[0]->index]
    && trav->node_connected[pair->nodes[1]->index]
    )
    {
        /* two fully connected nodes are linked, simply link the first particle.  */
        kd_enum_check(pair->nodes, trav->ll2, 1, _kd_fof_visit_edge_first, data);
    } else {
        kd_enum_check(pair->nodes, trav->ll2, 1, _kd_fof_visit_edge, data);
    }

    return 0;
}

static int
_kd_fof_visit_edge_first(void * data, KDEnumPair * pair) 
{
    _kd_fof_visit_edge_first(data, pair);
    return -1;
}

static int
_kd_fof_visit_edge(void * data, KDEnumPair * pair) 
{
    TraverseData * trav = (TraverseData *) data;

    ptrdiff_t i = pair->i;
    ptrdiff_t j = pair->j;

    trav->visited ++;

    ptrdiff_t root_i = splay(trav, i);
    ptrdiff_t root_j = splay(trav, j);

    /* merge root_j as direct subtree of the root */
    /* this is also correct if root_j == root_i */
    trav->head[root_j] = root_i;

    /* terminate immediately if two nodes are self-connected and
     * we have linked a pair*/
    return 0;
}

static double kd_node_maxdist2(KDNode * node)
{
    int d;
    double dist = 0;
    for(d = 0; d < 3; d ++) {
        double dx = kd_node_max(node)[d] - kd_node_min(node)[d];
        dist += dx * dx;
    }
    return dist;
}

static void
connect(TraverseData * trav, KDNode * node, int parent_connected)
{
    int c = 0;
    if(parent_connected) {
        c = 1;
    } else {
        if(kd_node_maxdist2(node) <= trav->ll2) {
            ptrdiff_t i;
            ptrdiff_t r = trav->ind[node->start];
            for(i = node->start + 1; i < node->size + node->start; i ++) {
                trav->head[trav->ind[i]] = r;
            }
            c = 1;
        }
    }
    trav->node_connected[node->index] = c;
    trav->connected += c;

    if(node->dim != -1) {
        connect(trav, node->link[0], c);
        connect(trav, node->link[1], c);
    }
}

static TraverseData last_traverse = {0};

int 
kd_fof(KDNode * node, double linking_length, ptrdiff_t * head)
{
    KDNode * nodes[2] = {node, node};
    TraverseData * trav = & (TraverseData) {};

    trav->head = head;
    trav->ll = linking_length;
    trav->ll2 = linking_length * linking_length;
    trav->node_connected = calloc(node->tree->size, 1);
    trav->ind = node->tree->ind;

    ptrdiff_t i;
    for(i = 0; i < node->size; i ++) {
        trav->head[i] = i;
    }

    trav->visited = 0;
    trav->maxdepth = 0;
    trav->totaldepth = 0;
    trav->nsplay = 0;
    trav->connected = 0;

    connect(trav, node, 0);

    kd_enum_always_open(nodes, linking_length, NULL, _kd_fof_check_nodes, trav);

    for(i = 0; i < node->size; i ++) {
        trav->head[i] = splay(trav, i);
    }

    free(trav->node_connected);

    last_traverse = *trav;
    return 0;
}

void
kd_fof_get_last_traverse_info(ptrdiff_t *visited, ptrdiff_t *connected,
                              ptrdiff_t *maxdepth, ptrdiff_t *nsplay, ptrdiff_t *totaldepth)
{
    *visited = last_traverse.visited;
    *connected = last_traverse.connected;
    *maxdepth = last_traverse.maxdepth;
    *nsplay = last_traverse.nsplay;
    *totaldepth = last_traverse.totaldepth;
}
