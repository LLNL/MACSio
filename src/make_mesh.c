#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef HAVE_MPI
#include <mpi.h>
#endif

#include <json.h>

#if 0
typedef MACSio_DjParCtx_t MPI_Comm;
typedef MACSio_ThParCtx_t 

typedef struct _MACSio_DomInfo_t {
    int64_t dpcglo_dom_id; /**< DPC-global domain id */
    int64_t dpc_rank;      /**< Rank of owning processor in the DPC */
} MACSio_DomNbor_t;

typedef struct _MACSio_DomDecomp_t {
    int64_t num_doms_total;           /**< Total number of domains over entire parallel context */
    MACSio_DisjParCtx_t disj_par_ctx; /**< Disjoint parallel context (DPC) */

    std::vector<MACSio_DomInfo_t>     /**< DPC-local domain information */
    int dpcloc_dom_cnt;               /**< DPC-local domain count. */
    MACSio_DomInfo_t const *

    int64_t const *dpcglo_dom_ids;    /**< Array of DBC-global domain ids */
    int64_t const *dpcloc_dom_nb_cnt; /**< Array of domain neighbor counts */
    MACSio_DomNbor_t const *dom_
    int64_t const * const *dpcglo_dom_nb_ids;    /**< Array of DBC-global domain neighbor ids */


    /* for each domain's neighbor, which MPI rank owns it (could be this one), local id in that rank */
    /* for each domain's neighbor, list of entities shared with that domain and local ids in that rank */
} MACSio_DomDecomp_t;
#endif

static int best_2d_factors(int val, int *x, int *y)
{
    int root = (int) sqrt((double)val);
    while (root)
    {
        if (!(val % root))
        {
            *x = root;
            *y = val / root;
            assert(*x * *y == val);
            return 0;
        }
        root--;
    }
    return 1;
}

static int best_3d_factors(int val, int *x, int *y, int *z)
{
    int root = (int) cbrt((double)val);
    int root2;
    int xb, yb, zb, mb=-1, mf=-1;

    /* first, walk backwards from the cube root */
    while (root)
    {
        if (!(val % root))
        {
            int val2d = val / root;
            if (!best_2d_factors(val2d, x, y))
            {
                *z = val / val2d;
                assert(*x * *y * *z == val);
                xb = *x;
                yb = *y;
                zb = *z;
                mb = xb<yb?xb:yb;
                mb = mb<zb?mb:zb;
                break;
            }
        }
        root--;
    }

    /* next, walk forwards from the cube root to the square root */
    root = (int) cbrt((double)val);
    root2 = (int) sqrt((double)val);
    while (root < root2)
    {
        if (!(val % root))
        {
            int val2d = val / root;
            if (!best_2d_factors(val2d, x, y))
            {
                *z = val / val2d;
                assert(*x * *y * *z == val);
                mf = *x<*y?*x:*y;
                mf = mf<*z?mf:*z;
                break;
            }
        }
        root++;
    }

    if (mb != -1)
    {
        if (mb > mf)
        {
            *x = xb;
            *y = yb;
            *z = zb;
        }
        return 0;
    }

    return 1;
}


extern char **enviornp;

int main(int argc, char **argv)
{
    int mpi_rank = 0;
    int mpi_size = 1;
    int argi = 0;
    int req_size = 80 * 1000; /* 80 kilo bytes ~equiv to 10,000 zones but avoid exact powers of 2. */
    int num_doms = -1;
    int num_doms_x, num_doms_y;
    domain_iterator_t *dom_it;
    json_object *macsio_clargs;
    json_object *macsio_object = json_object_new_object();

#ifdef HAVE_MPI
    MPI_Comm macsio_mpi_comm = MPI_COMM_WORLD;
#endif

#ifdef HAVE_MPI
    MPI_Init(&argc, &argv);
    if (MPI_Comm_dup(MPI_COMM_WORLD, &macsio_mpi_comm) != MPI_SUCCESS)
    {
    }
    MPI_Comm_size(macsio_mpi_comm, &mpi_size);
    MPI_Comm_rank(macsio_mpi_comm, &mpi_rank);
#endif 

    macsio_clargs = process_command_line_args(macsio_mpi_comm, argc, argv, enviornp);

#warning NAME PROGRAM VARIABLES TO DISTINGUISH BETWEEN LOCAL/GLOBAL VALUES

    /* package all CL args into a JSON array and broadcast it */
#warning NEED CL ARGS PACKAGE

    if (num_doms == -1)
        num_doms = mpi_size;

    /* Compute a suitable (ideally square) 2D decomposition. If num_doms is 
       a prime, then you get long y-strips. */
    best_2d_factors(num_doms, &num_doms_x, &num_doms_y);

    /* Get an iterator over domains on this rank */
    /* assign domains to MPI ranks (round robin for now) */
    /* create mesh boxes (1 per domain) */
    dom_it = get_iterator_for_domains_on_this_rank(mpi_size, mpi_rank,
                 num_doms_x, num_doms_y);

    macsio_problem_domains = json_object_mkdir(macsio_problem_object, "domains");

    while (dom_it->have_domains())
    {
        json_object_t *dom_json_obj;

        dom_it->get_domain_params();
  
        dom_json_obj = build_domain();

        json_object_object_add(macsio_problem_domains, dom_json_obj);
        
        dom_it->next_domain();
    }

    free_domain_iterator(dom_it);

    /* write the JSON-thingy */ 
    plugin_dump("Silo", macsio_problem_object);

#ifdef HAVE_MPI
    MPI_Finalize();
#endif

    return 0;
}
