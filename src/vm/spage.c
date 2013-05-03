// Turnin Date: 5/3/2013
//Name: Ryan Spring
//EID: rds2367
//CS login: rdspring
//Email: rdspring1@gmail.com
//Unique Number: 53426

//Name1: Ruben Fitch
//EID1: rsf293
//CS login: rubensam
//Email: rubensamf@utexas.edu
//Unique Number: 53435

//Name2: Josue Roman
//EID2: jgr397
//CS login: jroman
//Email: jozue.roman@gmail.com
//Unique Number: 53425

//Name3: Peter Farago
//EID2: pf3546
//CS login: farago
//Email: faragopeti@tx.rr.com
//Unique Number: 53435

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
