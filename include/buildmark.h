/* $Id$
 *
 * mICQ version functions and ICQ client identifiers.
 */

#ifndef MICQ_BUILDMARK_H
#define MICQ_BUILDMARK_H

#define MICQ_VERSION VERSION

#define BUILD_MICQ     0xffffff42L
#define BUILD_MICQ_OLD 0x7d0001eaL

const        char  *BuildVersion (void);
const        char  *BuildAttribution (void);
extern const UDWORD BuildVersionNum;
extern const char  *BuildVersionText;

#endif /* MICQ_BUILDMARK_H */
