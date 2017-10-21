#pragma once

/*
 * Copyright (c) 2014, Yung-Yu Chen <yyc@solvcon.net>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of the copyright holder nor the names of its contributors
 *   may be used to endorse or promote products derived from this software
 *   without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "march/mesh/UnstructuredBlock/class.hpp"

namespace march {

template< size_t NDIM >
void UnstructuredBlock<NDIM>::CEMesh::calc_ce(const UnstructuredBlock<NDIM> & block) {
    // references.
    const auto & ndcrd = block.ndcrd();
    const auto & fccnd = block.fccnd();
    const auto & fccls = block.fccls();
    const auto & fcnds = block.fcnds();
    const auto & clcnd = block.clcnd();
    const auto & clfcs = block.clfcs();
    // indices.
    index_type clnfc, fcnnd;
    // pointers.
    const index_type *pclfcs, *pfccls, *pfcnds;
    const real_type *pclcnd, *pfccnd, *pndcrd;
    real_type *pcevol, *p2cevol, *pcecnd, *p2cecnd;
    // vectors.
    real_type crdi[NDIM], crde[NDIM];
    real_type cndi[NDIM], cnde[NDIM];
    real_type crd[FCMND+2][NDIM];
    // scalars.
    real_type disu0, disu1, disu2, disv0, disv1, disv2;
    real_type dist0, dist1, dist2, disw0, disw1, disw2;
    real_type voli, vole, volb, volc;  // internal, external, BCE, CCE.
    // iterators.
    index_type icl, jcl, ind, ifl, inf, ifc;
    // loop for cells.
    for (icl=0; icl<block.ncell(); ++icl) {
        pcevol = &cevol[icl][0];
        pcecnd = &cecnd[icl][0];
        // self cell center.
        pclcnd = &clcnd[icl][0];
        crdi[0] = pclcnd[0];
        crdi[1] = pclcnd[1];
        if (NDIM == 3) {
            crdi[2] = pclcnd[2];
        }

        // loop for each face in cell.
        p2cevol = pcevol;
        p2cecnd = pcecnd;
        volc = 0.0;
        pcecnd[0] = 0.0;
        pcecnd[1] = 0.0;
        if (NDIM == 3) {
            pcecnd[2] = 0.0;
        }
        pclfcs = &clfcs[icl][0];
        clnfc = pclfcs[0];
        for (ifl=1; ifl<=clnfc; ++ifl) {
            ifc = pclfcs[ifl];
            pfccnd = &fccnd[ifc][0];
            pfccls = &fccls[ifc][0];
            jcl = pfccls[0] + pfccls[1] - icl;

            // neighbor cell center.
            pclcnd = &clcnd[jcl][0];
            crde[0] = pclcnd[0];
            crde[1] = pclcnd[1];
            if (NDIM == 3) {
                crde[2] = pclcnd[2];
            }

            // node coordinates.
            pfcnds = &fcnds[ifc][0];
            fcnnd = pfcnds[0];
            for (inf=1; inf<=fcnnd; ++inf) {
                ind = pfcnds[inf];
                pndcrd = &ndcrd[ind][0];
                crd[inf][0] = pndcrd[0];
                crd[inf][1] = pndcrd[1];
                if (NDIM == 3) {
                    crd[inf][2] = pndcrd[2];
                }
            }
            crd[fcnnd+1][0] = crd[1][0];
            crd[fcnnd+1][1] = crd[1][1];
            if (NDIM == 3) {
                crd[fcnnd+1][2] = crd[1][2];
            }

            // calculate volume and center of BCEs and in term CCEs.
            p2cevol += 1;
            p2cecnd += NDIM;
            if (NDIM == 3) {
                volb = 0.0;
                p2cecnd[0] = p2cecnd[1] = p2cecnd[2] = 0.0;
                for (inf=1; inf<=fcnnd; ++inf) {
                    // base triangle.
                    disu0 = crd[inf  ][0] - pfccnd[0];
                    disu1 = crd[inf  ][1] - pfccnd[1];
                    disu2 = crd[inf  ][2] - pfccnd[2];
                    disv0 = crd[inf+1][0] - pfccnd[0];
                    disv1 = crd[inf+1][1] - pfccnd[1];
                    disv2 = crd[inf+1][2] - pfccnd[2];
                    dist0 = disu1*disv2 - disu2*disv1;
                    dist1 = disu2*disv0 - disu0*disv2;
                    dist2 = disu0*disv1 - disu1*disv0;
                    // outer tetrahedron.
                    disw0 = crde[0] - pfccnd[0];
                    disw1 = crde[1] - pfccnd[1];
                    disw2 = crde[2] - pfccnd[2];
                    vole = fabs(dist0*disw0 + dist1*disw1 + dist2*disw2) / 6;
                    cnde[0] = (crd[inf][0]+crd[inf+1][0]+pfccnd[0] + crde[0]) / 4;
                    cnde[1] = (crd[inf][1]+crd[inf+1][1]+pfccnd[1] + crde[1]) / 4;
                    cnde[2] = (crd[inf][2]+crd[inf+1][2]+pfccnd[2] + crde[2]) / 4;
                    // inner tetrahedron.
                    disw0 = crdi[0] - pfccnd[0];
                    disw1 = crdi[1] - pfccnd[1];
                    disw2 = crdi[2] - pfccnd[2];
                    voli = fabs(dist0*disw0 + dist1*disw1 + dist2*disw2) / 6;
                    cndi[0] = (crd[inf][0]+crd[inf+1][0]+pfccnd[0] + crdi[0]) / 4;
                    cndi[1] = (crd[inf][1]+crd[inf+1][1]+pfccnd[1] + crdi[1]) / 4;
                    cndi[2] = (crd[inf][2]+crd[inf+1][2]+pfccnd[2] + crdi[2]) / 4;
                    // accumulate volume and centroid for BCE.
                    volb += voli + vole;
                    p2cecnd[0] += cndi[0]*voli + cnde[0]*vole;
                    p2cecnd[1] += cndi[1]*voli + cnde[1]*vole;
                    p2cecnd[2] += cndi[2]*voli + cnde[2]*vole;
                }
                volc += volb;
                pcecnd[0] += p2cecnd[0];
                pcecnd[1] += p2cecnd[1];
                pcecnd[2] += p2cecnd[2];
                p2cevol[0] = volb;
                p2cecnd[0] /= volb;
                p2cecnd[1] /= volb;
                p2cecnd[2] /= volb;
            } else {
                // triangle formed by cell point and two face nodes.
                cndi[0] = (crd[1][0] + crd[2][0] + crdi[0]) / 3;
                cndi[1] = (crd[1][1] + crd[2][1] + crdi[1]) / 3;
                voli = fabs((crd[1][0]-crdi[0])*(crd[2][1]-crdi[1])
                          - (crd[1][1]-crdi[1])*(crd[2][0]-crdi[0])) / 2;
                // triangle formed by neighbor cell point and two face nodes.
                cnde[0] = (crd[1][0] + crd[2][0] + crde[0]) / 3;
                cnde[1] = (crd[1][1] + crd[2][1] + crde[1]) / 3;
                vole = fabs((crd[1][0]-crde[0])*(crd[2][1]-crde[1])
                          - (crd[1][1]-crde[1])*(crd[2][0]-crde[0])) / 2;
                // volume of BCE (quadrilateral) formed by the two triangles.
                volb = voli + vole;
                p2cevol[0] = volb;
                // geometry center of each BCE for cell j.
                p2cecnd[0] = (cndi[0]*voli+cnde[0]*vole)/volb;
                p2cecnd[1] = (cndi[1]*voli+cnde[1]*vole)/volb;
                // volume and geometry center of the CCE for each cell.
                volc += volb;
                pcecnd[0] += p2cecnd[0]*volb;
                pcecnd[1] += p2cecnd[1]*volb;
            }
        }
        pcevol[0] = volc;
        pcecnd[0] /= volc;
        pcecnd[1] /= volc;
        if (NDIM == 3) {
            pcecnd[2] /= volc;
        }
    }
}

} /* end namespace march */

// vim: set ff=unix fenc=utf8 nobomb et sw=4 ts=4:
