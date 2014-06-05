#!/usr/bin/python
#
# sodtube1d.py
#
#
# 1D Sod Tube Test
#
# This program is implemented by OO style to be
# a part of ipython notebook demo materials.
#

import scipy.optimize as so

class PlotManager():
    """
    Manage how to show the data generated by SodTube.
    Roughly speaking, it is a wrapper of matplotlib
    """
    def __init__(self):
        pass

    def plotMesh(self, mesh):
        pass

    def plotSolution(self, solution):
        pass

class DataManager(PlotManager):
    """
    Manage how to get extended information by input data.
    """
    def __init__(self):
        pass

    def getErrorNorm(self, solution_A, solution_B):
        return solution_errornorm

    def getL2Norm(self, solution_A, solution_B):
        return solution_errornorm

class SodTube():
    """
    The core to generate the 1D Sod tube test
    """
    def __init__(self):
        # initial condition
        # [(rhol, ul, pl), (rhor, ur, pr)]
        #
        # Sod's initial condition
        self.RHOL = 1.0
        self.UL = 0.0
        self.PL = 1.0
        self.RHOR = 0.125
        self.UR = 0.0
        self.PR = 1.0
        self.initcondition_sod = [(self.RHOL, self.UL, self.PL),
                                  (self.RHOR, self.UR, self.PR)]
        # initial condition for a shock tube problem
        # default is Sod's initial condition
        # users could change this initial conditions
        self.initcondition = self.initcondition_sod
        # constants and conventions
        self.GAMMA = 1.4 # ideal gas constant
        self.GAMMA2 = (self.GAMMA - 1.0) / (self.GAMMA + 1.0)
        self.BETA = (self.GAMMA - 1.0) / (2.0*self.GAMMA)
        # a mesh, which has this format:
        # [point0, point1, point2, point3, ......, pointn]
        self.mesh = []
        # solution has this format:
        # [(x0, rho0, u0, p0),
        #  (x1, rho1, u1, p1),
        #  ......,
        #  (xn, rhon, un, pn)]
        self.solution = []
        self.ceseparameters = []

    def getInitcondition(self):
        return self.initcondition

    def setInitcondition(self, initcondition):
        self.initcondition = initcondition

    def getMesh(self):
        return self.mesh

    def getAnalyticSolution(self):
        return self.calAnalyticSolution()

    def calAnalyticSolution(self, initcondition=None):
        # where implementing the code to get the analytic solution
        # by users' input condition
        # default is the Sod's condition
        initcondition = initcondition or self.initcondition
        solution = []
        return solution

    def analyticVelocityRegionIV(self, x):
        gamma = self.GAMMA
        ushock = self.getVelocityShock()
        pr = self.PR
        pIV = self.getAnalyticPressureRegionIV()
        return x - (ushock/gamma)*(pIV/pr-1.0)*(((2*gamma/(gamma+1.0))/((pIV/pr)+(gamma-1.0)/(gamma+1.0)))**0.5)

    def analyticPressureRegionIV(self, x):
        gamma = self.GAMMA
        urwave = self.getVelocityRWave()
        ushock = self.getVelocityShock()
        pl = self.PL
        pr = self.PR
        return (x/pr)*(1-(gamma-1)*(urwave/ushock)*(x/pr-1)/((2*gamma*(2*gamma+(gamma+1)*(x/pr-1)))**0.5))**(-2*gamma/(gamma-1)) - pl/pr

    def getVelocityRWave(self):
        return ((self.GAMMA*self.PL/self.RHOL)**0.5)

    def getVelocityShock(self):
        return ((self.GAMMA*self.PR/self.RHOR)**0.5)

    def getVelocityRegionI(self):
        return self.UL

    def getVelocityRegionII(self):
        pass

    def getVelocityRegionIII(self):
        pass

    def getAnalyticVelocityRegionIV(self):
        return self.getAnalyticVelocityRegionIVByNewton()

    def getAnalyticVelocityRegionIVByNewton(self, x0=1):
        """
        x0 : the guess initial value to be applied in Newton method
        """
        return so.newton(self.analyticVelocityRegionIV,x0)

    def getVelocityRegionV(self):
        return self.UR

    def getPressureRegionI(self):
        pass

    def getPressureRegionII(self):
        pass

    def getPressureRegionIII(self):
        return self.getPressureRegionIV()

    def getAnalyticPressureRegionIV(self):
        return self.getAnalyticPressureRegionIVByNewton()

    def getAnalyticPressureRegionIVByNewton(self, x0=1):
        """
        x0 : the guess initial value to be applied in Newton method
        """
        return so.newton(self.analyticPressureRegionIV,x0)

    def getPressureRegionV(self):
        pass

    def getDensityRegionI(self):
        pass

    def getDensityRegionII(self):
        pass

    def getDensityRegionIII(self):
        pass

    def getDensityRegionIV(self):
        pass

    def getDensityRegionV(self):
        pass

    def getCESESolution(self):
        return self.solution

    def calCESESolution(self, initcondition, mesh, ceseparameters):
        return self.solution

