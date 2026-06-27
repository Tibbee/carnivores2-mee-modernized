// ==========================================================================
// CharacterTracking.cpp — Blood trail and scent tracking system
// ==========================================================================
// Extracted from Game.cpp (AddBloodTrail, AnimateBloodTrails).

#include "Hunt.h"

void AddBloodTrail(TCharacter *cptr)
{
  if (BloodTrail.Count > 508)
  {
    memcpy(&BloodTrail.Trail[0], &BloodTrail.Trail[1], 510 * sizeof(TBloodP));
    BloodTrail.Count--;
  }
  BloodTrail.Trail[BloodTrail.Count].Owner = cptr->CType;
  BloodTrail.Trail[BloodTrail.Count].LTime = 210000;
  BloodTrail.Trail[BloodTrail.Count].pos = cptr->pos;
  BloodTrail.Trail[BloodTrail.Count].pos.x += siRand(32);
  BloodTrail.Trail[BloodTrail.Count].pos.z += siRand(32);
  BloodTrail.Trail[BloodTrail.Count].pos.y =
    GetLandH(BloodTrail.Trail[BloodTrail.Count].pos.x,
             BloodTrail.Trail[BloodTrail.Count].pos.z) + 4;
  BloodTrail.Count++;
}

void AnimateBloodTrails()
{
  for (int b = 0; b < BloodTrail.Count; b++)
  {
    BloodTrail.Trail[b].LTime -= TimeDt;
    if (BloodTrail.Trail[b].LTime <= 0)
    {
      memcpy(&BloodTrail.Trail[b], &BloodTrail.Trail[b + 1], (511 - b) * sizeof(TBloodP));
      BloodTrail.Count--;
      b--;
    }
  }
}
