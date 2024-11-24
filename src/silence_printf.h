/* Silence printf for C. Include this file with -include directive to gcc to
 * "replace" printf with empty macro.
 */

#ifndef __cplusplus
#define printf(...)
#endif
