/* $ Id: $
 *
 * mICQ version functions and ICQ client identifiers.
 */

#define MICQ_VERSION VERSION

#define BUILD_MICQ     0xffffff42L
#define BUILD_MICQ_OLD 0x7d0001eaL
#define BUILD_MIRANDA  0xffffffffL
#define BUILD_STRICQ   0xffffff8fL
#define BUILD_YSM      0xffffffabL

#define BUILD_LICQ     0x7d000000L
#define BUILD_SSL      0x00800000L

const        char  *BuildVersion (void);
extern const UDWORD BuildVersionNum;
