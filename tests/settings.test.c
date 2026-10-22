#include "../src/c/kit/kit.h"
#include <assert.h>
static KitSettings saved;
static int saved_size;
int persist_get_size(uint32_t key) { assert(key==1000); return saved_size; }
int persist_read_data(uint32_t key,void *out,size_t n) {
  assert(key==1000); memcpy(out,&saved,n); return n;
}
int persist_write_data(uint32_t key,const void *data,size_t n) {
  assert(key==1000); memcpy(&saved,data,n); saved_size=n; return n;
}
Tuple *dict_find(DictionaryIterator *d,uint32_t key) { (void)d;(void)key;return NULL; }
bool clock_is_24h_style(void) { return true; }
void kit_world_fill(LabData *d,int zone) { (void)d;(void)zone; }
int main(void) {
  kit_settings_init(); assert(kit_settings_get()->step_goal==8000);
  saved=*kit_settings_get();saved.step_goal=10000;saved_size=sizeof(saved);
  kit_settings_init();assert(kit_settings_get()->step_goal==10000);
  saved.step_goal=12500;kit_settings_init();assert(kit_settings_get()->step_goal==12500);
  saved.step_goal=-1;kit_settings_init();assert(kit_settings_get()->step_goal==12500);
  puts("Watch settings passed: fresh 8,000 default, saved 10,000/custom goals preserved, corrupt record rejected.");
}
