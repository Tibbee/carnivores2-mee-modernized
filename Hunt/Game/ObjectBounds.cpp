// ofBOUND load-time fitting and player wall/floor/ceiling queries.
// Isolated from world traversal so tests exercise the production entry points.
#include "Hunt.h"
#include "Core/BoundFit.h"
#include <vector>

namespace {
constexpr float kRotCos[4] = {1.0f, 0.0f, -1.0f, 0.0f};
constexpr float kRotSin[4] = {0.0f, 1.0f, 0.0f, -1.0f};
}

void CheckBoundCollision(float &px, float &py, float cx, float cy, float oy, TBound *bound, int angle)
{
  float ppx=px-cx;
  float ppy=py-cy;

  float ca = kRotCos[angle & 3];
  float sa = kRotSin[angle & 3];
  float w,h;

  for (int o=0; o<8; o++)
  {

    if (bound[o].a<0) continue;
    if (bound[o].gx != 0.0f || bound[o].gz != 0.0f ||
        bound[o].bottomGx != 0.0f || bound[o].bottomGz != 0.0f)
    {
      // Evaluate both surfaces at the player's clamped local position:
      // the top keeps the step-over gate and the underside preserves
      // headroom beneath tilted slabs instead of filling a false wedge.
      // Either surface may be fitted independently; zero gradients are flat.
      float qx, qy;
      c2bound::LocalOf(px, py, cx, cy, bound[o].cx, bound[o].cy,
                       bound[o].cosph, bound[o].sinph, angle, qx, qy);
      const float qxC = c2bound::ClampF(qx, -bound[o].a, bound[o].a);
      const float qyC = c2bound::ClampF(qy, -bound[o].b, bound[o].b);
      const float top = c2bound::TopAt(bound[o].y2, bound[o].gx, bound[o].gz, qxC, qyC);
      const float bottom = c2bound::BottomAt(bound[o].y1, bound[o].bottomGx, bound[o].bottomGz, qxC, qyC);
      if (!c2bound::WallHeightOverlaps(top, bottom, oy, PlayerY)) continue;
      c2bound::FitBox fb;
      fb.cx = bound[o].cx;
      fb.cz = bound[o].cy;
      fb.a  = bound[o].a;
      fb.b  = bound[o].b;
      fb.cos = bound[o].cosph;
      fb.sin = bound[o].sinph;
      c2bound::WallPushOriented(px, py, cx, cy, fb, angle, 2.f);
      continue;
    }
    if (bound[o].y2 + oy < PlayerY + 128) continue;
    if (bound[o].y1 + oy > PlayerY + 256) continue;

    if (bound[o].cosph != 1.0f || bound[o].sinph != 0.0f)
    {
      // Fitted (oriented) box: same wall test in the box frame; the push is
      // rotated back (Core/BoundFit.h). Legacy boxes keep the inline path.
      c2bound::FitBox fb;
      fb.cx = bound[o].cx;
      fb.cz = bound[o].cy;
      fb.a  = bound[o].a;
      fb.b  = bound[o].b;
      fb.cos = bound[o].cosph;
      fb.sin = bound[o].sinph;
      c2bound::WallPushOriented(px, py, cx, cy, fb, angle, 2.f);
      continue;
    }
    float ccx = bound[o].cx*ca + bound[o].cy*sa;
    float ccy = bound[o].cy*ca - bound[o].cx*sa;

    if (angle & 1)
    {
      w = bound[o].b+2.f;
      h = bound[o].a+2.f;
    }
    else
    {
      w = bound[o].a+2.f;
      h = bound[o].b+2.f;
    }

    float dw = fabs(ppx - ccx) - w;
    float dh = fabs(ppy - ccy) - h;

    if ( (dw > 0) || (dh > 0) ) continue;

    if (dw > dh)
    {
      px = cx+ccx + w * SGN(ppx-ccx);
    }
    else
    {
      py = cy+ccy + h * SGN(ppy-ccy);
    }
  }
}

BOOL PointOnBound(float &H, float px, float py, float cx, float cy, float oy, TBound *bound, int angle)
{
  px-=cx;
  py-=cy;
  float ca = static_cast<float>(cos(angle*pi / 2.f));
  float sa = static_cast<float>(sin(angle*pi / 2.f));
  BOOL _on = false;
  H=-1000;
  for (int o=0; o<8; o++)
  {
    if (bound[o].a<0) continue;
    if (bound[o].gx != 0.0f || bound[o].gz != 0.0f)
    {
      // Sloped box: the floor is the top plane evaluated at the query point
      // (clamped for the step-up gate), so the player climbs the ramp
      // instead of being refused by the flat maximum height.
      float qx, qy;
      c2bound::LocalOf(px, py, 0.f, 0.f, bound[o].cx, bound[o].cy,
                       bound[o].cosph, bound[o].sinph, angle, qx, qy);
      const float qxC = c2bound::ClampF(qx, -bound[o].a, bound[o].a);
      const float qyC = c2bound::ClampF(qy, -bound[o].b, bound[o].b);
      if (c2bound::TopAt(bound[o].y2, bound[o].gx, bound[o].gz, qxC, qyC) + oy > PlayerY + 128) continue;
      if (fabs(qx) < bound[o].a && fabs(qy) < bound[o].b)
      {
        _on = true;
        const float top = c2bound::TopAt(bound[o].y2, bound[o].gx, bound[o].gz, qx, qy);
        if (H < top) H = top;
      }
      continue;
    }
    if (bound[o].y2 + oy > PlayerY + 128) continue;
    BOOL inside;
    if (bound[o].cosph != 1.0f || bound[o].sinph != 0.0f)
    {
      // Fitted box: containment in the box frame (Core/BoundFit.h); px/py
      // are already relative to the cell centre.
      c2bound::FitBox fb;
      fb.cx = bound[o].cx;
      fb.cz = bound[o].cy;
      fb.a  = bound[o].a;
      fb.b  = bound[o].b;
      fb.cos = bound[o].cosph;
      fb.sin = bound[o].sinph;
      inside = c2bound::OrientedContains(px, py, 0.f, 0.f, fb, angle);
    }
    else
    {
      float a,b;
      float ccx = bound[o].cx*ca + bound[o].cy*sa;
      float ccy = bound[o].cy*ca - bound[o].cx*sa;
      if (angle & 1)
      {
        a = bound[o].b;
        b = bound[o].a;
      }
      else
      {
        a = bound[o].a;
        b = bound[o].b;
      }
      inside = ( fabs(px - ccx) < a) && (fabs(py - ccy) < b);
    }
    if (inside)
    {
      _on=true;
      if (H < bound[o].y2) H = bound[o].y2;
    }
  }
  return _on;
}
BOOL PointUnBound(float &H, float px, float py, float cx, float cy, float oy, TBound *bound, int angle)
{
  px-=cx;
  py-=cy;
  float ca = static_cast<float>(cos(angle*pi / 2.f));
  float sa = static_cast<float>(sin(angle*pi / 2.f));
  BOOL _on = false;
  // A fitted underside may exceed 1000 model units; do not cap its height.
  H=FLT_MAX;
  for (int o=0; o<8; o++)
  {
    if (bound[o].a<0) continue;
    if (bound[o].bottomGx != 0.0f || bound[o].bottomGz != 0.0f)
    {
      // The ceiling follows the independently fitted underside, not the
      // part-wide minimum. Same box frame and strict containment as floors.
      float qx, qy;
      c2bound::LocalOf(px, py, 0.f, 0.f, bound[o].cx, bound[o].cy,
                       bound[o].cosph, bound[o].sinph, angle, qx, qy);
      if (fabs(qx) < bound[o].a && fabs(qy) < bound[o].b)
      {
        const float bottom = c2bound::BottomAt(
          bound[o].y1, bound[o].bottomGx, bound[o].bottomGz, qx, qy);
        if (bottom + oy < PlayerY + 128) continue;
        _on = true;
        if (H > bottom) H = bottom;
      }
      continue;
    }
    if (bound[o].y1 + oy < PlayerY + 128) continue;
    BOOL inside;
    if (bound[o].cosph != 1.0f || bound[o].sinph != 0.0f)
    {
      // Fitted box: containment in the box frame (Core/BoundFit.h); px/py
      // are already relative to the cell centre.
      c2bound::FitBox fb;
      fb.cx = bound[o].cx;
      fb.cz = bound[o].cy;
      fb.a  = bound[o].a;
      fb.b  = bound[o].b;
      fb.cos = bound[o].cosph;
      fb.sin = bound[o].sinph;
      inside = c2bound::OrientedContains(px, py, 0.f, 0.f, fb, angle);
    }
    else
    {
      float a,b;
      float ccx = bound[o].cx*ca + bound[o].cy*sa;
      float ccy = bound[o].cy*ca - bound[o].cx*sa;
      if (angle & 1)
      {
        a = bound[o].b;
        b = bound[o].a;
      }
      else
      {
        a = bound[o].a;
        b = bound[o].b;
      }
      inside = ( fabs(px - ccx) < a) && (fabs(py - ccy) < b);
    }
    if (inside)
    {
      _on=true;
      if (H > bound[o].y1) H = bound[o].y1;
    }
  }
  return _on;
}

void CalcBoundBox(TModel* mptr, TBound *bound)
{
  float x1 = 0.f, x2 = 0.f, y1 = 0.f, y2 = 0.f, z1 = 0.f, z2 = 0.f;
  std::vector<float> fitX, fitZ;   // per-part footprint for the optional fit
  std::vector<float> fitY;         // per-part heights for the optional slope fit
  BOOL first;

  for (int o=0; o<8; o++)
  {
    first = true;
    bound[o].a=-1;
    bound[o].cosph = 1.0f;
    bound[o].sinph = 0.0f;
    bound[o].gx = 0.0f;
    bound[o].gz = 0.0f;
    bound[o].bottomGx = 0.0f;
    bound[o].bottomGz = 0.0f;
    fitX.clear();
    fitZ.clear();
    fitY.clear();

    for (int v=0; v<mptr->VCount; v++)
    {
      TPoint3d p = mptr->gVertex[v];
      if (p.hide) continue;
      if (p.owner!=o) continue;
      fitX.push_back(p.x);
      fitZ.push_back(p.z);
      fitY.push_back(p.y);

      if (first)
      {
        x1 = p.x-1.0f;
        x2 = p.x+1.0f;
        y1 = p.y-1.0f;
        y2 = p.y+1.0f;
        z1 = p.z-1.0f;
        z2 = p.z+1.0f;
        first = false;
      }

      if (p.x < x1) x1=p.x;
      if (p.x > x2) x2=p.x;

      if (p.y < y1) y1=p.y;
      if (p.y > y2) y2=p.y;

      if (p.z < z1) z1=p.z;
      if (p.z > z2) z2=p.z;
    }

    if (first) continue;

    x1-=72.f;
    x2+=72.f;
    z1-=72.f;
    z2+=72.f;

    bound[o].y1 = y1;
    bound[o].y2 = y2;
    bound[o].cx = (x1+x2) / 2;
    bound[o].cy = (z1+z2) / 2;
    bound[o].a  = (x2-x1) / 2;
    bound[o].b  = (z2-z1) / 2;
    // Experimental oriented fit (config.cfg "boundbox_fit", default off):
    // the fitted box replaces the AABB only when it wins by the configured
    // percentage, so axis-aligned and organic parts keep the legacy values.
    if (OptBoundBoxFit == 1 && fitX.size() >= 2)
    {
      const int count = static_cast<int>(fitX.size());
      const c2bound::FitBox fit = c2bound::FitMinArea(fitX.data(), fitZ.data(), count, 72.f);
      if (c2bound::FitAccepts(fit.Area(), bound[o].a * bound[o].b * 4.f, OptBoundBoxFitWin))
      {
        bound[o].cx = fit.cx;
        bound[o].cy = fit.cz;
        bound[o].a  = fit.a;
        bound[o].b  = fit.b;
        bound[o].cosph = fit.cos;
        bound[o].sinph = fit.sin;
      }
    }
    // Experimental sloped surfaces (config.cfg "boundbox_slope", default off):
    // fit upper/lower envelopes independently in the chosen footprint frame.
    // Failed/flat fits retain legacy limits; accepted undersides cannot cross
    // the retained top anywhere within the padded box.
    if (OptBoundBoxSlope != 0 && fitX.size() >= 3)
    {
      const float c = bound[o].cosph;
      const float s = bound[o].sinph;
      const int count = static_cast<int>(fitX.size());
      std::vector<float> localU(count);
      std::vector<float> localV(count);
      for (int i2=0; i2<count; ++i2)
      {
        localU[i2] = fitX[i2] * c + fitZ[i2] * s;
        localV[i2] = fitZ[i2] * c - fitX[i2] * s;
      }
      const float cu = bound[o].cx * c + bound[o].cy * s;
      const float cv = bound[o].cy * c - bound[o].cx * s;
      const c2bound::SlopedBounds surfaces = c2bound::FitSlopedBounds(
        localU.data(), localV.data(), fitY.data(), count, cu, cv,
        bound[o].a, bound[o].b, bound[o].y1, bound[o].y2);
      bound[o].y1 = surfaces.y1;
      bound[o].y2 = surfaces.y2;
      bound[o].gx = surfaces.gx;
      bound[o].gz = surfaces.gz;
      bound[o].bottomGx = surfaces.bottomGx;
      bound[o].bottomGz = surfaces.bottomGz;
    }

  }

}
