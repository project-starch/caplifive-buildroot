/* Internal per-open ownership. VMAs and dup/fork retain the owning struct file. */
struct process_owner {
    bool managed;
    bool used;
};

static DEFINE_MUTEX(capstone_lock);
/* The legacy API exposes global IDs and raw mappings. It cannot safely coexist
 * with per-file ownership. Select one interface for the module lifetime. */
static bool process_api_selected, legacy_api_selected;
static void process_release(struct process_owner *owner);
static int process_mmap(struct process_owner *owner, struct vm_area_struct *vma);
