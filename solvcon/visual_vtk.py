# -*- coding: UTF-8 -*-
#
# Copyright (C) 2010 Yung-Yu Chen <yyc@solvcon.net>.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

"""
Visualizing algorithm by VTK.
"""

VANMAP = dict(float32='vtkFloatArray', float64='vtkDoubleArray')

def make_ust_from_blk(blk):
    """
    Create vtk.vtkUnstructuredGrid object from Block object.

    @param blk: input block.
    @type blk: solvcon.block.Block
    @return: the ust object.
    @rtype: vtk.vtkUnstructuredGrid
    """
    from numpy import array
    import vtk
    cltpm = array([1, 3, 9, 5, 12, 10, 13, 14], dtype='int32')
    nnode = blk.nnode
    ndcrd = blk.ndcrd
    ncell = blk.ncell
    cltpn = blk.cltpn
    clnds = blk.clnds
    # create vtkPoints.
    pts = vtk.vtkPoints()
    pts.SetNumberOfPoints(nnode)
    ind = 0
    while ind < nnode:
        pts.SetPoint(ind, ndcrd[ind,:])
        ind += 1
    # create vtkUnstructuredGrid.
    ust = vtk.vtkUnstructuredGrid()
    ust.Allocate(ncell, ncell)
    ust.SetPoints(pts)
    icl = 0
    while icl < ncell:
        tpn = cltpm[cltpn[icl]]
        ids = vtk.vtkIdList()
        for inl in range(1, clnds[icl,0]+1):
            ids.InsertNextId(clnds[icl,inl])
        ust.InsertNextCell(tpn, ids)
        icl += 1
    return ust

def valid_vector(arr):
    """
    A valid vector must have 3 compoments.  If it has only 2, pad it.  If
    it has more than 3, raise ValueError.

    @param arr: input vector array.
    @type arr: numpy.ndarray
    @return: validated array.
    @rtype: numpy.ndarray
    """
    from numpy import empty
    if arr.shape[1] < 3:
        arrn = empty((arr.shape[0], 3), dtype=arr.dtype)
        arrn[:,2] = 0.0
        try:
            arrn[:,:2] = arr[:,:]
        except ValueError, e:
            args = e.args[:]
            args.append('arrn.shape=%s, arr.shape=%s' % (
                str(arrn.shape), str(arr.shape)))
            e.args = args
            raise
        arr = arrn
    elif arr.shape[1] > 3:
        raise IndexError('arr.shape[1] = %d > 3'%arr.shape[1])
    return arr

def set_array(arr, name, fpdtype, ust):
    """
    Set the data of a ndarray to vtk array and return the set vtk array.
    If the array of the specified name existed, use the existing array.

    @param arr: input array.
    @type arr: numpy.ndarray
    @param name: array name.
    @type name: str
    @param fpdtype: floating point dtype.
    @type fpdtype: str
    @param ust: the UnstructuredGrid object to store the array.
    @type ust: vtk.vtkobject
    @return: the set VTK array object.
    """
    import vtk
    if ust.GetCellData().HasArray(name):
        vaj = ust.GetCellData().GetArray(name)
    else:
        vaj = getattr(vtk, VANMAP[fpdtype])()
        # prepare for vector.
        if len(arr.shape) > 1:
            vaj.SetNumberOfComponents(3)
        # set number of tuples to allocate.
        vaj.SetNumberOfTuples(arr.shape[0])
        # cache.
        vaj.SetName(name)
        ust.GetCellData().AddArray(vaj)
    # set data.
    nt = arr.shape[0]
    it = 0
    if len(arr.shape) > 1:
        while it < nt:
            vaj.SetTuple3(it, *arr[it])
            it += 1
    else:
        while it < nt:
            vaj.SetValue(it, arr[it])
            it += 1
    return vaj

class VtkOperation(object):
    """
    Pre-defined VTK operations.
    """
    @staticmethod
    def c2p(inp):
        """
        VTK operation: cell to point.

        @param inp: input VTK object.
        @type inp: vtk.vtkobject
        @return: output VTK object.
        @rtype: vtk.vtkobject
        """
        import vtk
        usp = vtk.vtkCellDataToPointData()
        usp.SetInput(inp)
        return usp
    @staticmethod
    def cut(inp, origin, normal):
        """
        VTK operation: cut.

        @param inp: input VTK object.
        @type inp: vtk.vtkobject
        @param origin: a 3-tuple for cut origin.
        @type origin: tuple
        @param normal: a 3-tuple for cut normal.
        @type normal: tuple
        @return: output VTK object.
        @rtype: vtk.vtkobject
        """
        import vtk
        pne = vtk.vtkPlane()
        pne.SetOrigin(origin)
        pne.SetNormal(normal)
        cut = vtk.vtkCutter()
        cut.SetInputConnection(inp.GetOutputPort())
        cut.SetCutFunction(pne)
        return cut
    @staticmethod
    def contour_value(inp, num, value):
        """
        VTK operation: contour by a single value.

        @param inp: input VTK object.
        @type inp: vtk.vtkobject
        @param num: the index of the contour line.
        @type num: int
        @param value: the value of the contour line.
        @type value: float
        @return: output VTK object.
        @rtype: vtk.vtkobject
        """
        import vtk
        cnr = vtk.vtkContourFilter()
        cnr.SetInputConnection(inp.GetOutputPort())
        cnr.SetValue(num, value)
        return cnr
    @staticmethod
    def contour_range(inp, num, begin, end):
        """
        VTK operation: contour by a range.

        @param inp: input VTK object.
        @type inp: vtk.vtkobject
        @param num: the number of the contour lines.
        @type num: int
        @param begin: the start of the range.
        @type begin: float
        @param end: the end of the range.
        @type end: float
        @return: output VTK object.
        @rtype: vtk.vtkobject
        """
        import vtk
        cnr = vtk.vtkContourFilter()
        cnr.SetInputConnection(inp.GetOutputPort())
        cnr.GenerateValues(num, begin, end)
        return cnr
    @staticmethod
    def lump_poly(*args):
        """
        Lump all passed-in poly together.  FIXME: when lumped together, vectors
        are messed up.

        @return: output VTK object.
        @rtype: vtk.vtkobject
        """
        import vtk
        apd = vtk.vtkAppendPolyData()
        for vbj in args:
            apd.AddInput(vbj.GetOutput())
        return apd
    @staticmethod
    def write_poly(inp, outfn):
        """
        Write VTK polydata to a file.

        @param inp: input VTK object.
        @type inp: vtk.vtkobject
        @param outfn: output file name.
        @type outfn: str
        @return: nothing
        """
        import vtk
        wtr = vtk.vtkXMLPolyDataWriter()
        wtr.EncodeAppendedDataOff()
        wtr.SetInput(inp.GetOutput())
        wtr.SetFileName(outfn)
        wtr.Write()
Vop = VtkOperation