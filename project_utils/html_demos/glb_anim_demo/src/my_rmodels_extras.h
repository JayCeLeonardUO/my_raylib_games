#ifndef MY_RMODELS_EXTRAS_H
#define MY_RMODELS_EXTRAS_H

#include <raylib.h>

// Like raylib's UpdateModelAnimation(), but correctly applies
// framePoses[frame][boneId].scale in the bone matrix (proper SRT order)
// and applies scale via Vector3Multiply during the vertex skinning loop.
void UpdateModelAnimationWithScale(Model model, ModelAnimation anim, int frame);

#endif // MY_RMODELS_EXTRAS_H
