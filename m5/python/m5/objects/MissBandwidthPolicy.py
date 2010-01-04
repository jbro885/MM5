from m5 import *

class RequestEstimationMethod(Enum): vals = ['MWS', 'MLP']
class PerformanceEstimationMethod(Enum): vals = ['latency-mlp', 'ratio-mws', "latency-mlp-sreq", "no-mlp"]

class MissBandwidthPolicy(SimObject):
    type = 'MissBandwidthPolicy'
    abstract = True
    period = Param.Tick("The number of clock cycles between each decision")
    interferenceManager = Param.InterferenceManager("The system's interference manager")
    cpuCount = Param.Int("The number of cores in the system")
    busUtilizationThreshold = Param.Double("The actual bus utilzation to consider the bus as full")
    requestCountThreshold = Param.Double("The request intensity (requests / tick) to assume no request increase")
    requestEstimationMethod = Param.RequestEstimationMethod("The request estimation method to use")
    performanceEstimationMethod = Param.PerformanceEstimationMethod("The method to use for performance estimations")
    persistentAllocations = Param.Bool("Use persistent allocations")
    acceptanceThreshold = Param.Double("The performance improvement needed to accept new MHA")
    requestVariationThreshold = Param.Double("Maximum acceptable request variation")
    renewMeasurementsThreshold = Param.Int("Samples to keep MHA")
