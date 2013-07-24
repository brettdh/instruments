#!/usr/bin/env python

import sys
import unittest
import numpy as np
import scipy.stats as stats
from matplotlib import pyplot as plt

def fit_weibull(values):
    '''Returns (shape, location, scale) parameters for a Weibull distribution 
    fit to the values using MLE.
    '''
    shape, location, scale = stats.weibull_min.fit(values, loc=0.0)
    return shape, location, scale


def goodness_of_fit(shape, location, scale, values):
    '''Returns log-likelihood of the distribution given the observed values.'''
    return 0.0

def run_tests():
    pass

def cdf_xvalues(max_x):
    max_x *= 1.05
    return np.arange(0.0, max_x * 1.001, max_x * 0.001)

def plot_fit(samples):
    shape, loc, scale = fit_weibull(samples)
    x = cdf_xvalues(max(samples))

    print "   fit: (shape=%f loc=%f scale=%f)" % (shape, loc, scale)
    fit_dist = stats.weibull_min(shape, loc=loc, scale=scale)
    fit_cdf_values = fit_dist.cdf(x)

    num_samples = len(samples)
    cdf = np.arange(1, num_samples + 1) / float(num_samples)
    
    plt.plot(sorted(samples), cdf, color="red", linestyle='none', marker='.', label='samples')
    plt.plot(x, fit_cdf_values, color="blue", marker='none', label='fit')


def generate_weibull_samples(num_samples=100):
    fixed_shape = 2.0
    fixed_loc = 0.0
    fixed_scale = 4.0
    print "actual: (shape=%f loc=%f scale=%f)" % (fixed_shape, fixed_loc, fixed_scale)
    weibull = stats.weibull_min(fixed_shape, loc=fixed_loc, scale=fixed_scale)

    max_value = 10.0
    
    x = cdf_xvalues(max_value)
    cdf_values = weibull.cdf(x)
    samples = weibull.rvs(num_samples)

    plt.plot(x, cdf_values, color="green", marker='none', label='actual')
    return samples

def main():
    #values = [int(x.strip()) for x in readlines(sys.stdin)]
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--gen-samples", type=int, metavar="NUM_SAMPLES")
    parser.add_argument("--stdin", action="store_true", default=False)
    args = parser.parse_args()
    if args.gen_samples:
        samples = generate_weibull_samples(args.gen_samples)
    elif args.stdin:
        samples = [float(x.strip()) for x in readlines(sys.stdin)]
    else:
        samples = generate_weibull_samples()
        
    plot_fit(samples)

    plt.xlabel("value")
    plt.ylabel("CDF")
    plt.legend(loc=4)
    plt.show()

if __name__ == '__main__':
    main()
