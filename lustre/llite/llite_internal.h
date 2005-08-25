/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright (C) 2003 Cluster File Systems, Inc.
 *
 * This code is issued under the GNU General Public License.
 * See the file COPYING in this distribution
 */

#ifndef LLITE_INTERNAL_H
#define LLITE_INTERNAL_H

#include <linux/lustre_debug.h>
#include <linux/lustre_audit.h>

/* default to about 40meg of readahead on a given system.  That much tied
 * up in 512k readahead requests serviced at 40ms each is about 1GB/s. */
#define SBI_DEFAULT_RA_MAX ((40 << 20) >> PAGE_CACHE_SHIFT)

enum ra_stat {
        RA_STAT_HIT = 0,
        RA_STAT_MISS,
        RA_STAT_DISTANT_READPAGE,
        RA_STAT_MISS_IN_WINDOW,
        RA_STAT_FAILED_MATCH,
        RA_STAT_DISCARDED,
        RA_STAT_ZERO_LEN,
        RA_STAT_ZERO_WINDOW,
        RA_STAT_EOF,
        RA_STAT_MAX_IN_FLIGHT,
        _NR_RA_STAT,
};

struct ll_ra_info {
        unsigned long             ra_cur_pages;
        unsigned long             ra_max_pages;
        unsigned long             ra_stats[_NR_RA_STAT];
};

/* after roughly how long should we remove an inactive mount? */
#define GNS_MOUNT_TIMEOUT 120

/* how often should the GNS timer look for mounts to cleanup? */
#define GNS_TICK_TIMEOUT  1

/* how many times GNS will try to wait for 1 second for mount */
#define GNS_WAIT_ATTEMPTS 10

struct ll_sb_info {
        /* this protects pglist and max_r_a_pages.  It isn't safe to grab from
         * interrupt contexts. */
        spinlock_t                ll_lock;

        struct obd_uuid           ll_sb_uuid;
        struct obd_export        *ll_md_exp;
        struct obd_export        *ll_dt_exp;
        struct lov_desc           ll_dt_desc;
        struct proc_dir_entry    *ll_proc_root;
        struct lustre_id          ll_rootid;     /* root lustre id */
                
        struct lustre_mount_data *ll_lmd;
        char                     *ll_instance;

        int                       ll_flags;
        struct list_head          ll_conn_chain; /* per-conn chain of SBs */

        struct hlist_head         ll_orphan_dentry_list; /*please don't ask -p*/
        struct ll_close_queue    *ll_lcq;

        struct lprocfs_stats     *ll_stats;      /* lprocfs stats counter */

        unsigned long             ll_pglist_gen;
        struct list_head          ll_pglist;

        struct ll_ra_info         ll_ra_info;

        unsigned int              ll_remote;      /* remote client? */
        
        __u64                     ll_audit_mask;
        /* times spent waiting for locks in each call site.  These are
         * all protected by the ll_lock */
        struct obd_service_time   ll_read_stime;
        struct obd_service_time   ll_write_stime;
        struct obd_service_time   ll_grouplock_stime;
        struct obd_service_time   ll_seek_stime;
        struct obd_service_time   ll_setattr_stime;
        struct obd_service_time   ll_brw_stime;
//      struct obd_service_time   ll_done_stime;

        int                       ll_config_version; /* last-applied update */

        /* list of GNS mounts; protected by the dcache_lock */
        struct list_head          ll_mnt_list;

        struct semaphore          ll_gns_sem;
        spinlock_t                ll_gns_lock;
        wait_queue_head_t         ll_gns_waitq;
        atomic_t                  ll_gns_enabled;
        int                       ll_gns_state;
        struct timer_list         ll_gns_timer;
        struct list_head          ll_gns_sbi_head;
        struct completion         ll_gns_mount_finished;
	struct dentry            *ll_gns_pending_dentry;

        unsigned long             ll_gns_tick;
        unsigned long             ll_gns_timeout;

        /* path to upcall */
        char                      ll_gns_upcall[PATH_MAX];

        /* mount object entry name */
        char                      ll_gns_oname[PATH_MAX];
        void                      *ll_crypto_info;

#if 0
        /* TODO: to support multi mount for capability */
        struct list_head          ll_capa_list;
        struct timer_list         ll_capa_timer;
#endif
};

struct ll_gns_ctl {
        struct completion gc_starting;
        struct completion gc_finishing;
};

/* mounting states */
#define LL_GNS_IDLE               (1 << 0)
#define LL_GNS_MOUNTING           (1 << 1)
#define LL_GNS_FINISHED           (1 << 2)

/* mounts checking flags */
#define LL_GNS_UMOUNT             (1 << 0)
#define LL_GNS_CHECK              (1 << 1)

/*
 * per file-descriptor read-ahead data.
 */
struct ll_readahead_state {
        spinlock_t      ras_lock;
        /*
         * index of the last page that read(2) needed and that wasn't in the
         * cache. Used by ras_update() to detect seeks.
         *
         * XXX nikita: if access seeks into cached region, Lustre doesn't see
         * this.
         */
        unsigned long   ras_last_readpage;
        /*
         * number of pages read after last read-ahead window reset. As window
         * is reset on each seek, this is effectively a number of consecutive
         * accesses. Maybe ->ras_accessed_in_window is better name.
         *
         * XXX nikita: window is also reset (by ras_update()) when Lustre
         * believes that memory pressure evicts read-ahead pages. In that
         * case, it probably doesn't make sense to expand window to
         * PTLRPC_MAX_BRW_PAGES on the third access.
         */
        unsigned long   ras_consecutive;
        /*
         * Parameters of current read-ahead window. Handled by
         * ras_update(). On the initial access to the file or after a seek,
         * window is reset to 0. After 3 consecutive accesses, window is
         * expanded to PTLRPC_MAX_BRW_PAGES. Afterwards, window is enlarged by
         * PTLRPC_MAX_BRW_PAGES chunks up to ->ra_max_pages.
         */
        unsigned long   ras_window_start, ras_window_len;
        /*
         * Where next read-ahead should start at. This lies within read-ahead
         * window. Read-ahead window is read in pieces rather than at once
         * because: 1. lustre limits total number of pages under read-ahead by
         * ->ra_max_pages (see ll_ra_count_get()), 2. client cannot read pages
         * not covered by DLM lock.
         */
        unsigned long   ras_next_readahead;

};

extern kmem_cache_t *ll_file_data_slab;
extern kmem_cache_t *ll_intent_slab;

struct lustre_handle;

struct ll_file_data {
        struct ll_readahead_state fd_ras;
        __u32 fd_flags;
        int fd_omode;
        struct lustre_handle fd_cwlockh;
        unsigned long fd_gid;
};

struct lov_stripe_md;

extern spinlock_t inode_lock;

extern void lprocfs_unregister_mountpoint(struct ll_sb_info *sbi);
extern struct proc_dir_entry *proc_lustre_fs_root;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
# define hlist_del_init list_del_init
#endif

static inline struct inode *ll_info2i(struct ll_inode_info *lli)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
        return &lli->lli_vfs_inode;
#else
        return list_entry(lli, struct inode, u.generic_ip);
#endif
}


struct it_cb_data {
        struct inode *icbd_parent;
        struct dentry **icbd_childp;
        obd_id hash;
};

#define LLAP_MAGIC 98764321

struct ll_async_page {
        int              llap_magic;
        void            *llap_cookie;
        struct page     *llap_page;
        struct list_head llap_pending_write;
        /* only trust these if the page lock is providing exclusion */
        unsigned         llap_write_queued:1,
                         llap_defer_uptodate:1,
                         llap_origin:3,
                         llap_ra_used:1;

        /* used to find proper capability */
        uid_t            llap_fsuid;

        struct list_head llap_proc_item;
};

enum {
        LLAP_ORIGIN_UNKNOWN = 0,
        LLAP_ORIGIN_READPAGE,
        LLAP_ORIGIN_READAHEAD,
        LLAP_ORIGIN_COMMIT_WRITE,
        LLAP_ORIGIN_WRITEPAGE,
        LLAP__ORIGIN_MAX,
};

/*
 * remote ACL stuff
 */
#define REMOTE_ACL_HASHSIZE     16

struct remote_acl {
        struct list_head        ra_perm_cache[REMOTE_ACL_HASHSIZE];
        spinlock_t              ra_lock;
        /* we use one sem per inode, it's kind of coarse: one user must
         * wait if another user is updating the perm on this inode. but
         * I guess this is fine is real world usage.
         */
        struct semaphore        ra_update_sem;
};

struct lustre_remote_perm {
        struct list_head        lrp_list;
        uid_t                   lrp_auth_uid;       /* authenticated uid */
        gid_t                   lrp_auth_gid;       /* authenticated gid */
        uint16_t                lrp_perm;           /* permission bits */
        uint16_t                lrp_valid:1,        /* lrp_perm is valid */
                                lrp_setuid:1,       /* allow setuid */
                                lrp_setgid:1;       /* allow setgid */
        struct list_head        lrp_setxid_perms;   /* setxid perms list */
};

struct remote_perm_setxid {
        struct list_head        list; /* permission list */
        uid_t                   uid;
        gid_t                   gid;
        uint16_t                perm;
};

/* llite/lproc_llite.c */
int lprocfs_register_mountpoint(struct proc_dir_entry *parent,
                                struct super_block *sb, char *lov,
                                char *lmv);
void lprocfs_unregister_mountpoint(struct ll_sb_info *sbi);

/* llite/dir.c */
extern struct file_operations ll_dir_operations;
extern struct inode_operations ll_dir_inode_operations;

/* llite/namei.c */
int ll_objects_destroy(struct ptlrpc_request *request,
                       struct inode *dir, int offset);
struct inode *ll_iget(struct super_block *sb, ino_t hash,
                      struct lustre_md *lic);
struct dentry *ll_find_alias(struct inode *, struct dentry *);
int ll_mdc_blocking_ast(struct ldlm_lock *, struct ldlm_lock_desc *,
                        void *data, int flag);
/* llite/rw.c */
int ll_prepare_write(struct file *, struct page *, unsigned from, unsigned to);
int ll_commit_write(struct file *, struct page *, unsigned from, unsigned to);
int ll_writepage(struct page *page);
void ll_inode_fill_obdo(struct inode *inode, int cmd, struct obdo *oa);
void ll_ap_completion(void *data, int cmd, struct obdo *oa, int rc);
void ll_removepage(struct page *page);
int ll_readpage(struct file *file, struct page *page);
struct ll_async_page *llap_from_cookie(void *cookie);
struct ll_async_page *llap_from_page(struct page *page, unsigned origin);
void ll_readahead_init(struct inode *inode, struct ll_readahead_state *ras);

struct lpage_data *lpd_cast_private(struct page *page);

void ll_ra_accounting(struct page *page, struct address_space *mapping);
void ll_truncate(struct inode *inode);

/* llite/file.c */
extern struct file_operations ll_file_operations;
extern struct inode_operations ll_file_inode_operations;
int ll_md_real_close(struct obd_export *md_exp,
                     struct inode *inode, int flags);
extern int ll_inode_revalidate_it(struct dentry *);
int ll_setxattr(struct dentry *, const char *, const void *,
                size_t, int);
int ll_getxattr(struct dentry *, const char *, void *, size_t);
int ll_listxattr(struct dentry *, char *, size_t);
int ll_removexattr(struct dentry *, const char *);
extern int ll_inode_permission(struct inode *, int, struct nameidata *);
int ll_get_acl(struct inode *, struct posix_acl **acl,
               struct ptlrpc_request **req);
int ll_refresh_lsm(struct inode *inode, struct lov_stripe_md *lsm);
int ll_extent_lock(struct ll_file_data *, struct inode *,
                   struct lov_stripe_md *, int mode, ldlm_policy_data_t *,
                   struct lustre_handle *, int ast_flags,
                   struct obd_service_time *);
int ll_extent_unlock(struct ll_file_data *, struct inode *,
                     struct lov_stripe_md *, int mode, struct lustre_handle *);
int ll_file_open(struct inode *inode, struct file *file);
int ll_file_release(struct inode *inode, struct file *file);
int ll_lsm_getattr(struct obd_export *, struct lov_stripe_md *, struct obdo *);
int ll_glimpse_size(struct inode *inode);
int ll_local_open(struct file *file, struct lookup_intent *it,
                  struct obd_client_handle *och);
int ll_md_close(struct obd_export *md_exp, struct inode *inode,
                struct file *file);
int ll_md_och_close(struct obd_export *md_exp, struct inode *inode,
                    struct obd_client_handle *och, int dirty, __u64 epoch);
int ll_och_fill(struct inode *inode, struct lookup_intent *it,
                struct obd_client_handle *och);
int ll_set_audit(struct inode *, __u64);
int ll_audit_log(struct inode *, audit_op, int);

int ll_getxattr_internal(struct inode *inode, const char *name,
                         void *value, size_t size, __u64 valid);
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0))
int ll_getattr(struct vfsmount *mnt, struct dentry *de, struct kstat *stat);
#endif
void ll_stime_record(struct ll_sb_info *sbi, struct timeval *start,
                     struct obd_service_time *stime);

/* llite_capa.c */
int ll_capa_thread_start(void);
void ll_capa_thread_stop(void);

void ll_capa_timer_callback(unsigned long unused);
int ll_set_capa(struct inode *inode, struct lookup_intent *it);
int ll_set_trunc_capa(struct ptlrpc_request *req, int offset,
                      struct inode *inode);

/* llite/dcache.c */
void ll_intent_drop_lock(struct lookup_intent *);
void ll_intent_release(struct lookup_intent *);
int ll_intent_alloc(struct lookup_intent *);
void ll_intent_free(struct lookup_intent *it);
extern void ll_set_dd(struct dentry *de);
void ll_unhash_aliases(struct inode *);
void ll_frob_intent(struct lookup_intent **itp, struct lookup_intent *deft);
void ll_lookup_finish_locks(struct lookup_intent *it, struct dentry *dentry);
int revalidate_it_finish(struct ptlrpc_request *request, int offset,
                         struct lookup_intent *it, struct dentry *de);


/* llite/llite_gns.c */
int ll_gns_thread_start(void);
void ll_gns_thread_stop(void);

int ll_gns_mount_object(struct dentry *dentry,
                        struct vfsmount *mnt);
int ll_gns_umount_object(struct vfsmount *mnt);

int ll_gns_check_mounts(struct ll_sb_info *sbi,
                        int flags);

void ll_gns_timer_callback(unsigned long data);
void ll_gns_add_timer(struct ll_sb_info *sbi);
void ll_gns_del_timer(struct ll_sb_info *sbi);

/* llite/llite_lib.c */
extern struct super_operations lustre_super_operations;

char *ll_read_opt(const char *opt, char *data);
int ll_set_opt(const char *opt, char *data, int fl);
void ll_options(char *options, char **ost, char **mds, char **gss,
                char **mds_sec, char **oss_sec, int *async, int *flags);
void ll_lli_init(struct ll_inode_info *lli);
int ll_fill_super(struct super_block *sb, void *data, int silent);
int lustre_fill_super(struct super_block *sb, void *data, int silent);
void lustre_put_super(struct super_block *sb);
struct inode *ll_inode_from_lock(struct ldlm_lock *lock);
void ll_clear_inode(struct inode *inode);
int ll_attr2inode(struct inode *inode, struct iattr *attr, int trunc);
int ll_setattr_raw(struct inode *inode, struct iattr *attr);
int ll_setattr(struct dentry *de, struct iattr *attr);
int ll_statfs(struct super_block *sb, struct kstatfs *sfs);
int ll_statfs_internal(struct super_block *sb, struct obd_statfs *osfs,
                       unsigned long maxage);
void ll_update_inode(struct inode *inode, struct lustre_md *);
int ll_fetch_remote_perm(struct inode *inode, struct ptlrpc_request *req,
                         uint16_t *perm);
int it_disposition(struct lookup_intent *it, int flag);
void it_set_disposition(struct lookup_intent *it, int flag);
void ll_read_inode2(struct inode *inode, void *opaque);
void ll_delete_inode(struct inode *inode);
int ll_iocontrol(struct inode *inode, struct file *file,
                 unsigned int cmd, unsigned long arg);
void ll_umount_begin(struct super_block *sb);
int ll_prep_inode(struct obd_export *, struct obd_export *, struct inode **inode,
                  struct ptlrpc_request *req, int offset, struct super_block *);
__u32 get_uuid2int(const char *name, int len);
struct dentry *ll_fh_to_dentry(struct super_block *sb, __u32 *data, int len,
                               int fhtype, int parent);
int ll_dentry_to_fh(struct dentry *, __u32 *datap, int *lenp, int need_parent);
int null_if_equal(struct ldlm_lock *lock, void *data);
int ll_process_config_update(struct ll_sb_info *sbi, int clean);
int ll_show_options(struct seq_file *m, struct vfsmount *mnt);
int ll_flush_cred(struct inode *inode);

int ll_remote_acl_permission(struct inode *inode, int mode);
int ll_remote_acl_update(struct inode *inode, struct mds_remote_perm *perm);
void ll_inode_invalidate_acl(struct inode *inode);

/* llite/special.c */
extern struct inode_operations ll_special_inode_operations;
extern struct file_operations ll_special_chr_inode_fops;
extern struct file_operations ll_special_chr_file_fops;
extern struct file_operations ll_special_blk_inode_fops;
extern struct file_operations ll_special_fifo_inode_fops;
extern struct file_operations ll_special_fifo_file_fops;
extern struct file_operations ll_special_sock_inode_fops;

/* llite/symlink.c */
extern struct inode_operations ll_fast_symlink_inode_operations;

/* llite/llite_close.c */
struct ll_close_queue {
        spinlock_t              lcq_lock;
        struct list_head        lcq_list;
        wait_queue_head_t       lcq_waitq;
        struct completion       lcq_comp;
        int                     lcq_stop;
};

void llap_write_pending(struct inode *inode, struct ll_async_page *llap);
void llap_write_complete(struct inode *inode, struct ll_async_page *llap);
void ll_open_complete(struct inode *inode);
int ll_is_inode_dirty(struct inode *inode);
void ll_try_done_writing(struct inode *inode);
void ll_queue_done_writing(struct inode *inode);
void ll_close_thread_stop(struct ll_close_queue *lcq);
int ll_close_thread_start(struct ll_close_queue **lcq_ret);


/* llite/llite_mmap.c */
#if  (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
typedef struct rb_root  rb_root_t;
typedef struct rb_node  rb_node_t;
#endif

struct ll_lock_tree_node {
        rb_node_t               lt_node;
        struct list_head        lt_locked_item;
        __u64                   lt_oid;
        ldlm_policy_data_t      lt_policy;
        struct lustre_handle    lt_lockh;
        ldlm_mode_t             lt_mode;
};

struct ll_lock_tree {
        rb_root_t                       lt_root;
        struct list_head                lt_locked_list;
        struct ll_file_data             *lt_fd;
};
int ll_teardown_mmaps(struct address_space *mapping, __u64 first,
                      __u64 last);
int ll_file_mmap(struct file * file, struct vm_area_struct * vma);
struct ll_lock_tree_node * ll_node_from_inode(struct inode *inode, __u64 start,
                                              __u64 end, ldlm_mode_t mode);
int ll_tree_lock(struct ll_lock_tree *tree,
                 struct ll_lock_tree_node *first_node, struct inode *inode,
                 const char *buf, size_t count, int ast_flags);
int ll_tree_unlock(struct ll_lock_tree *tree, struct inode *inode);

int ll_get_fid(struct obd_export *exp, struct lustre_id *idp,
               char *filename, struct lustre_id *ret);

/* generic */
#define LL_SBI_NOLCK           0x1
#define LL_SBI_READAHEAD       0x2
#define LL_SUPER_MAGIC         0x0BD00BD0
#define LL_MAX_BLKSIZE         (4UL * 1024 * 1024)

#if  (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
#define    ll_s2sbi(sb)        ((struct ll_sb_info *)((sb)->s_fs_info))
#define    ll_set_sbi(sb, sbi) ((sb)->s_fs_info = sbi)
static inline __u64 ll_ts2u64(struct timespec *time)
{
        __u64 t = time->tv_sec;
        return t;
}
#else  /* 2.4 here */
#define    ll_s2sbi(sb)        ((struct ll_sb_info *)((sb)->u.generic_sbp))
#define    ll_set_sbi(sb, sbi) ((sb)->u.generic_sbp = sbi)
static inline __u64 ll_ts2u64(time_t *time)
{
        return *time;
}
#endif

/* don't need an addref as the sb_info should be holding one */
static inline struct obd_export *ll_s2dtexp(struct super_block *sb)
{
        return ll_s2sbi(sb)->ll_dt_exp;
}

/* don't need an addref as the sb_info should be holding one */
static inline struct obd_export *ll_s2mdexp(struct super_block *sb)
{
        return ll_s2sbi(sb)->ll_md_exp;
}

static inline struct client_obd *sbi2md(struct ll_sb_info *sbi)
{
        struct obd_device *obd = sbi->ll_md_exp->exp_obd;
        if (obd == NULL)
                LBUG();
        return &obd->u.cli;
}

// FIXME: replace the name of this with LL_SB to conform to kernel stuff
static inline struct ll_sb_info *ll_i2sbi(struct inode *inode)
{
        return ll_s2sbi(inode->i_sb);
}

static inline struct obd_export *ll_i2dtexp(struct inode *inode)
{
        return ll_s2dtexp(inode->i_sb);
}

static inline struct obd_export *ll_i2mdexp(struct inode *inode)
{
        return ll_s2mdexp(inode->i_sb);
}
static inline int ll_mds_max_easize(struct super_block *sb)
{
        return sbi2md(ll_s2sbi(sb))->cl_max_mds_easize;
}

static inline __u64 ll_file_maxbytes(struct inode *inode)
{
        return ll_i2info(inode)->lli_maxbytes;
}


static inline void
ll_inode2id(struct lustre_id *id, struct inode *inode)
{
        struct lustre_id *lid = &ll_i2info(inode)->lli_id;

        mdc_pack_id(id, inode->i_ino, inode->i_generation,
                    (inode->i_mode & S_IFMT), id_group(lid),
                    id_fid(lid));
}

static inline void
ll_inode2mdc_data(struct mdc_op_data *op_data,
                  struct inode *inode,
                  obd_valid valid)
{
        obd_valid newvalid = 0;
        
        LASSERT(op_data != NULL);
        LASSERT(inode != NULL);

        /* put object id there all the time. */
        if (valid & OBD_MD_FLID) {
                ll_inode2id(&op_data->id1, inode);
                newvalid |= OBD_MD_FLID;
        }
        
        /* it could be directory with mea */
        if (valid & OBD_MD_MEA) {
                op_data->mea1 = ll_i2info(inode)->lli_mea;
                if (op_data->mea1)
                        newvalid |= OBD_MD_MEA;
        }

        if (valid & OBD_MD_FLSIZE) {
                op_data->size = inode->i_size;
                newvalid |= OBD_MD_FLSIZE;
        }
        if (valid & OBD_MD_FLBLOCKS) {
                op_data->blocks = inode->i_blocks;
                newvalid |= OBD_MD_FLBLOCKS;
        }
        if (valid & OBD_MD_FLFLAGS) {
                op_data->flags = inode->i_flags;
                newvalid |= OBD_MD_FLFLAGS;
        }
        if (valid & OBD_MD_FLATIME) {
                op_data->atime = LTIME_S(inode->i_atime);
                newvalid |= OBD_MD_FLATIME;
        }
        if (valid & OBD_MD_FLMTIME) {
                op_data->mtime = LTIME_S(inode->i_mtime);
                newvalid |= OBD_MD_FLMTIME;
        }
        if (valid & OBD_MD_FLCTIME) {
                op_data->ctime = LTIME_S(inode->i_ctime);
                newvalid |= OBD_MD_FLCTIME;
        }
        if (valid & OBD_MD_FLTYPE) {
                op_data->mode = (op_data->mode & S_IALLUGO) |
                        (inode->i_mode & S_IFMT);
                newvalid |= OBD_MD_FLTYPE;
        }

        if (valid & OBD_MD_FLMODE) {
                op_data->mode = (op_data->mode & S_IFMT) |
                        (inode->i_mode & S_IALLUGO);
                newvalid |= OBD_MD_FLMODE;
        }

        op_data->valid |= newvalid;
}

static inline void
ll_prepare_mdc_data(struct mdc_op_data *op_data, struct inode *i1,
                    struct inode *i2, const char *name, int namelen,
                    int mode)
{
        LASSERT(op_data != NULL);
        LASSERT(i1 != NULL);
        
        ll_inode2id(&op_data->id1, i1);

        /* it could be directory with mea */
        op_data->mea1 = ll_i2info(i1)->lli_mea;

        if (i2) {
                ll_inode2id(&op_data->id2, i2);
                op_data->mea2 = ll_i2info(i2)->lli_mea;
        }

	op_data->valid = 0;
        op_data->name = name;
        op_data->namelen = namelen;
        op_data->create_mode = mode;
        op_data->mod_time = LTIME_S(CURRENT_TIME);
}

struct crypto_helper_ops {
       int (*init_it_key)(struct inode *inode, struct lookup_intent *it);
       int (*create_key)(struct inode *dir, mode_t mode, void **key,
                         int* key_size);
       int (*get_mac)(struct inode *inode, struct iattr *iattr, void*value,
                      int size, void **key, int* key_size);
       int (*decrypt_key)(struct inode *inode, struct lookup_intent *it);
       int (*init_inode_key)(struct inode *inode, void *md_key);
       int (*destroy_key)(struct inode *inode);
};

/*llite crypto ops for crypto api*/
struct ll_crypto_info {
        struct obd_export    *ll_gt_exp;
        struct list_head     ll_cops_list;
        struct crypto_helper_ops *ll_cops;
        int    ll_c_flags;
};
#define ll_page2key(page)  ((ll_i2info(page->mapping->host))->lli_key_info)
static inline struct ll_crypto_info* ll_s2crpi(struct super_block *sb)
{
        return (struct ll_crypto_info*)ll_s2sbi(sb)->ll_crypto_info;
}
static inline struct ll_crypto_info* ll_i2crpi(struct inode *inode)
{
        return (struct ll_crypto_info*)ll_i2sbi(inode)->ll_crypto_info;
}

static inline struct crypto_helper_ops* ll_i2crpops(struct inode *inode)
{
        return ll_i2crpi(inode)->ll_cops;
}

static inline struct crypto_helper_ops* ll_s2crpops(struct super_block *sb)
{
        return ll_s2crpi(sb)->ll_cops;
}

static inline struct obd_export *ll_s2gsexp(struct super_block *sb)
{
        struct ll_crypto_info *llci = ll_s2crpi(sb);
        if (llci)
                return llci->ll_gt_exp;
        return NULL;
}
static inline struct obd_export *ll_i2gsexp(struct inode *inode)
{
        return ll_s2gsexp(inode->i_sb);
}

static inline
int ll_crypto_init_it_key(struct inode *inode, struct lookup_intent *it)
{
        struct ll_crypto_info *lci = ll_i2crpi(inode);
        if (lci) {
                struct crypto_helper_ops *ops = lci->ll_cops;;
                if (ops && ops->init_it_key)
                        return ops->init_it_key(inode, it);
        }
        return 0;
}

static inline
int ll_crypto_create_key(struct inode *inode, mode_t mode, void **key,
                         int* key_size)
{
        struct ll_crypto_info *lci = ll_i2crpi(inode);
        if (lci) {
                struct crypto_helper_ops *ops = lci->ll_cops;;
                if (ops && ops->create_key)
                        return ops->create_key(inode, mode, key, key_size);
        }
        return 0;
}

static inline
int ll_crypto_get_mac(struct inode *inode, struct iattr *attr, void *acl,
                      int acl_size, void **mac, int *mac_size)
{
        struct ll_crypto_info *lci = ll_i2crpi(inode);
        if (lci) {
                struct crypto_helper_ops *ops = lci->ll_cops;
                if (ops && ops->get_mac)
                        return ops->get_mac(inode, attr, acl, acl_size,
                                            mac, mac_size);
        }
        return 0;
}

static inline
int ll_crypto_decrypt_key(struct inode *inode, struct lookup_intent *it)
{
        struct ll_crypto_info *lci = ll_i2crpi(inode);
        if (lci) {
                struct crypto_helper_ops *ops = lci->ll_cops;
                if (ops && ops->decrypt_key)
                        return ops->decrypt_key(inode, it);
        }
        return 0;
}

static inline
int ll_crypto_init_inode_key(struct inode *inode, void *md_key)
{
        struct ll_crypto_info *lci = ll_i2crpi(inode);
        if (lci) {
                struct crypto_helper_ops *ops = lci->ll_cops;
                if (ops && ops->init_inode_key)
                        return ops->init_inode_key(inode, md_key);
        }
        return 0;
}

static inline
int ll_crypto_destroy_inode_key(struct inode *inode)
{
        struct ll_crypto_info *lci = ll_i2crpi(inode);
        if (lci) {
                struct crypto_helper_ops *ops = lci->ll_cops;
                if (ops && ops->destroy_key)
                        return ops->destroy_key(inode);
        }
        return 0;
}

int lustre_init_crypto(struct super_block *sb, char *gkc,
                       struct obd_connect_data *data, int async);
int lustre_destroy_crypto(struct super_block *sb);
int ll_set_sb_gksinfo(struct super_block *sb, char *type);
/* pass this flag to ll_md_real_close() to send close rpc right away */
#define FMODE_SYNC               00000010
#endif /* LLITE_INTERNAL_H */
