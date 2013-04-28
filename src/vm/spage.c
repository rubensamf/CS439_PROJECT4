#include "vm/spage.h"
#include "lib/kernel/hash.h"

unsigned
spage_hash_hash_func (const struct hash_elem *e, void *aux UNUSED)
{
  const struct spage *p = hash_entry (e, struct spage, hash_elem);
  return hash_bytes (&p->addr, sizeof p->addr);
}

/* Returns true if page a precedes page b. */
bool
spage_hash_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
  const struct spage *aspage = hash_entry (a, struct spage, hash_elem);
  const struct spage *bspage = hash_entry (b, struct spage, hash_elem);
  return aspage->addr < bspage->addr;
}

/* Returns the page containing the given virtual address,
   or a null pointer if no such page exists. */
struct spage *
spage_lookup (struct hash * pages, const void *address)
{
  struct spage p;
  struct hash_elem *e;
  p.addr = address;
  e = hash_find (pages, &p.hash_elem);
  return e != NULL ? hash_entry (e, struct spage, hash_elem) : NULL;
}

/* Deletes and returns the page containing the given virtual address,
   or a null pointer if no such page exists. */
struct spage *
spage_delete (struct hash * pages, const void *address)
{
  struct spage p;
  struct hash_elem *e;
  p.addr = address;
  e = hash_delete (pages, &p.hash_elem);
  return e != NULL ? hash_entry (e, struct spage, hash_elem) : NULL;
}
