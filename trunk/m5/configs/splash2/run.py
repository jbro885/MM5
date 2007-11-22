from m5 import *
import Splash2

if 'SYSTEM' not in env:
    panic("The SYSTEM environment variable must be set!\ne.g -ESYSTEM=Detailed\n")

if env['SYSTEM'] == 'Simple':
    from SimpleConfig import *    
    BaseCPU.workload = Parent.workload
    SimpleStandAlone.cpu = [ CPU() for i in xrange(int(env['NP'])) ]
    root = SimpleStandAlone()
elif env['SYSTEM'] == 'Detailed':
    from DetailedConfig import *    
    BaseCPU.workload = Parent.workload
    DetailedStandAlone.cpu = [ DetailedCPU() for i in xrange(int(env['NP'])) ]
    root = DetailedStandAlone()
else:
    panic("The SYSTEM environment variable was set to something improper.\n Use Simple or Detailed\n")

if 'BENCHMARK' not in env:
        panic("The BENCHMARK environment variable must be set!\ne.g. -EBENCHMARK=Cholesky\n")

if env['BENCHMARK'] == 'Cholesky':
    root.workload = Splash2.Cholesky()
elif env['BENCHMARK'] == 'FFT':
    root.workload = Splash2.FFT()
elif env['BENCHMARK'] == 'LUContig':
    root.workload = Splash2.LU_contig()
elif env['BENCHMARK'] == 'LUNoncontig':
    root.workload = Splash2.LU_noncontig()
elif env['BENCHMARK'] == 'Radix':
    root.workload = Splash2.Radix()
elif env['BENCHMARK'] == 'Barnes':
    root.workload = Splash2.Barnes()
elif env['BENCHMARK'] == 'FMM':
    root.workload = Splash2.FMM()
elif env['BENCHMARK'] == 'OceanContig':
    root.workload = Splash2.Ocean_contig()
elif env['BENCHMARK'] == 'OceanNoncontig':
    root.workload = Splash2.Ocean_noncontig()
elif env['BENCHMARK'] == 'Raytrace':
    root.workload = Splash2.Raytrace()
elif env['BENCHMARK'] == 'WaterNSquared':
    root.workload = Splash2.Water_nsquared()
elif env['BENCHMARK'] == 'WaterSpatial':
    root.workload = Splash2.Water_spatial()
else:
    panic("The BENCHMARK environment variable was set to something" \
          +" improper.\nUse Cholesky, FFT, LUContig, LUNoncontig, Radix" \
          +", Barnes, FMM, OceanContig,\nOceanNoncontig, Raytrace," \
          +" WaterNSquared, or WaterSpatial\n")
