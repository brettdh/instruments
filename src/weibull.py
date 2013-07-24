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
    shape, location, scale = stats.weibull_min.fit(values, loc=0.0, floc=0.0)
    return shape, location, scale


def goodness_of_fit(shape, location, scale, values):
    '''Returns log-likelihood of the distribution given the observed values.'''
    return 0.0

def run_tests():
    pass

def cdf_xvalues(max_x):
    max_x *= 1.05
    return np.arange(0.0, max_x * 1.001, max_x * 0.001)

def failure_prob(distribution, samples, failure_window):
    '''Pr(X < upper | X >= lower)
       = Pr(X < upper ^ X >= lower) / Pr(X >= lower)
       = ( F(upper) - F(lower) ) / (1 - F(lower))
    '''
    cdf = distribution.cdf
    lower = samples - failure_window
    lower[lower < 0] = 0.0
    upper = samples
    return ( cdf(upper) - cdf(lower)) / (1.0 - cdf(lower))

def plot_fit(samples, failure_window):
    shape, loc, scale = fit_weibull(samples)
    x = cdf_xvalues(max(samples))

    print "   fit: (shape=%f loc=%f scale=%f)" % (shape, loc, scale)
    fit_dist = stats.weibull_min(shape, loc=loc, scale=scale)
    fit_cdf_values = fit_dist.cdf(x)

    num_samples = len(samples)
    cdf = np.arange(1, num_samples + 1) / float(num_samples)
    
    plt.plot(sorted(samples), cdf, color="black", linestyle='none', marker='.', label='samples')
    plt.plot(x, fit_cdf_values, color="blue", marker='none', label='fit CDF')

    if failure_window:
        failure_pdf = failure_prob(fit_dist, x, failure_window)
        max_i, max_val = np.argmax(failure_pdf), np.max(failure_pdf)
        print "Max failure prob %f at time %f" % (max_val, x[max_i])
        plt.plot(x, failure_pdf, color="red", marker='none', 
                 label="%d-second\nfailure PDF" % failure_window)

def generate_weibull_samples(num_samples=100, params=None):
    fixed_shape = 2.0
    fixed_loc = 0.0
    fixed_scale = 4.0
    if params:
        fixed_shape, fixed_scale = params

    print "actual: (shape=%f loc=%f scale=%f)" % (fixed_shape, fixed_loc, fixed_scale)
    weibull = stats.weibull_min(fixed_shape, loc=fixed_loc, scale=fixed_scale)

    max_value = 10.0
    
    x = cdf_xvalues(max_value)
    cdf_values = weibull.cdf(x)
    samples = weibull.rvs(num_samples)

    plt.plot(x, cdf_values, color="green", marker='none', label='actual CDF')
    return samples

def main():
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--gen-samples", type=int, metavar="NUM_SAMPLES")
    parser.add_argument("--params", type=float, nargs=2, metavar=['SHAPE', 'SCALE'])
    parser.add_argument("--stdin", action="store_true", default=False)
    parser.add_argument("--failure-window", type=int)
    parser.add_argument("--kaist-paper-params", action="store_true", default=False)
    args = parser.parse_args()
    if args.gen_samples:
        samples = generate_weibull_samples(args.gen_samples, args.params)
    elif args.stdin:
        samples = [float(x.strip()) for x in sys.stdin.readlines()]
    elif args.kaist_paper_params:
        samples = generate_weibull_samples(100000, params=[0.52, 6.17 * np.e - 4])
        if args.failure_window:
            args.failure_window /= 3600.0 # seconds to hours
            print "Converted failure window to %f hours" % args.failure_window
        
    else:
        samples = generate_weibull_samples(params=args.params)
        
    plot_fit(samples, args.failure_window)

    plt.xlabel("value")
    plt.legend(loc=4)
    plt.show()

if __name__ == '__main__':
    main()
