import sys
import os.path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from kdcount import correlate
import numpy

#pos = numpy.float32(numpy.fromfile('A00_hodfit.raw', dtype='f8').reshape(-1,
#    8)[::20, 0:3]).copy()
#numpy.save('TEST-A00_hodfit-small.npy', pos)

def test_p():
    pos = numpy.load(os.path.join(os.path.dirname(__file__),
        'TEST-A00_hodfit-small.npy'))

    dataset = correlate.points(pos, boxsize=1.0)
    binning = correlate.RBinning(0.1, Nbins=20)
    r = correlate.paircount(dataset, dataset, binning, np=0)
    print r.centers, r.sum1
def test_np():
    pos = numpy.load(os.path.join(os.path.dirname(__file__),
        'TEST-A00_hodfit-small.npy'))

    dataset = correlate.points(pos, boxsize=None)
    binning = correlate.RBinning(0.1, Nbins=20)
    r = correlate.paircount(dataset, dataset, binning, np=0)
    print r.centers, r.sum1

def test_p_w():
    pos = numpy.load(os.path.join(os.path.dirname(__file__),
        'TEST-A00_hodfit-small.npy'))
    w = numpy.ones((len(pos))) 
    dataset = correlate.points(pos, boxsize=1.0, weight=w)
    binning = correlate.RBinning(0.1, Nbins=20)
    r = correlate.paircount(dataset, dataset, binning, np=0)
    print r.centers, r.sum1

test_p()
test_np()
test_p_w()
