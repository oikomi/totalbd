/*
 * apm_trace.c
 *
 *  Created on: 15/08/2015
 *      Author: miaohong(miaohong01@baidu.com)
 */

/*
 * Start profiling - called just before calling the actual function
 * NOTE:  PLEASE MAKE SURE TSRMLS_CC IS AVAILABLE IN THE CONTEXT
 *        OF THE FUNCTION WHERE THIS MACRO IS CALLED.
 *        TSRMLS_CC CAN BE MADE AVAILABLE VIA TSRMLS_DC IN THE
 *        CALLING FUNCTION OR BY CALLING TSRMLS_FETCH()
 *        TSRMLS_FETCH() IS RELATIVELY EXPENSIVE.
 */
#define BEGIN_PROFILING(entries, symbol, profile_curr)                  \
  do {                                                                  \
    /* Use a hash code to filter most of the string comparisons. */     \
    uint8 hash_code  = hp_inline_hash(symbol);                          \
    profile_curr = !hp_ignore_entry(hash_code, symbol);                 \
    if (profile_curr) {                                                 \
      hp_entry_t *cur_entry = hp_fast_alloc_hprof_entry();              \
      (cur_entry)->hash_code = hash_code;                               \
      (cur_entry)->name_hprof = symbol;                                 \
      (cur_entry)->prev_hprof = (*(entries));                           \
      /* Call the universal callback */                                 \
      hp_mode_common_beginfn((entries), (cur_entry) TSRMLS_CC);         \
      /* Call the mode's beginfn callback */                            \
      hp_globals.mode_cb.begin_fn_cb((entries), (cur_entry) TSRMLS_CC); \
      /* Update entries linked list */                                  \
      (*(entries)) = (cur_entry);                                       \
    }                                                                   \
  } while (0)

/*
 * Stop profiling - called just after calling the actual function
 * NOTE:  PLEASE MAKE SURE TSRMLS_CC IS AVAILABLE IN THE CONTEXT
 *        OF THE FUNCTION WHERE THIS MACRO IS CALLED.
 *        TSRMLS_CC CAN BE MADE AVAILABLE VIA TSRMLS_DC IN THE
 *        CALLING FUNCTION OR BY CALLING TSRMLS_FETCH()
 *        TSRMLS_FETCH() IS RELATIVELY EXPENSIVE.
 */
#define END_PROFILING(entries, profile_curr)                            \
  do {                                                                  \
    if (profile_curr) {                                                 \
      hp_entry_t *cur_entry;                                            \
      /* Call the mode's endfn callback. */                             \
      /* NOTE(cjiang): we want to call this 'end_fn_cb' before */       \
      /* 'hp_mode_common_endfn' to avoid including the time in */       \
      /* 'hp_mode_common_endfn' in the profiling results.      */       \
      hp_globals.mode_cb.end_fn_cb((entries) TSRMLS_CC);                \
      cur_entry = (*(entries));                                         \
      /* Call the universal callback */                                 \
      hp_mode_common_endfn((entries), (cur_entry) TSRMLS_CC);           \
      /* Free top entry and update entries linked list */               \
      (*(entries)) = (*(entries))->prev_hprof;                          \
      hp_fast_free_hprof_entry(cur_entry);                              \
    }                                                                   \
  } while (0)