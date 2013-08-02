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
    print "   fit: (shape=%f loc=%f scale=%f)" % (shape, location, scale)

    return stats.weibull_min(shape, loc=location, scale=scale)

def goodness_of_fit(shape, location, scale, values):
    '''Returns log-likelihood of the distribution given the observed values.'''
    return 0.0

def run_tests():
    pass

def cdf_xvalues(max_x):
    max_x *= 1.05
    num_samples = 10000
    return np.arange(0.0, max_x * (1 + (1.0 / num_samples)), max_x / num_samples)

def failure_prob(distribution, samples, failure_window, tx_time=0.0):
    '''
       p(failure) = Pr(X < cur_x + tx_time + failure_window | X >= cur_x)
       
       Pr(X < upper | X >= lower)
       = Pr(X < upper ^ X >= lower) / Pr(X >= lower)
       = ( F(upper) - F(lower) ) / (1 - F(lower))

       upper = cur_x + failure_window
       lower = cur_x
    '''
    cdf = distribution.cdf
    lower = samples
    upper = samples + failure_window
    return ( cdf(upper) - cdf(lower)) / (1.0 - cdf(lower))

def failure_penalty(failure_probs, failure_window):
    return failure_probs * failure_window / 2.0

max_value = None

def plot_fit(samples, failure_window, xlabel="value"):
    fit_dist = fit_weibull(samples)
    if max_value:
        x = cdf_xvalues(max_value)
    else:
        x = cdf_xvalues(max(samples))

    fit_cdf_values = fit_dist.cdf(x)

    num_samples = len(samples)
    samples = [0.0] + samples
    cdf = np.arange(num_samples + 1) / float(num_samples)
    

    if max_value:
        x_to_plot = x[x <= max_value]
        samples_to_plot = filter(lambda v: v <= max_value, sorted(samples))
    else:
        x_to_plot = x
        samples_to_plot = sorted(samples)

    fit_cdf_values_to_plot = fit_cdf_values[:len(x_to_plot)]
    cdf_to_plot = cdf[:len(samples_to_plot)]
    
    plt.step(samples_to_plot, cdf_to_plot, where="post", color="black", #linestyle='none',
             marker='.', label='samples')
    plt.plot(x_to_plot, fit_cdf_values_to_plot, color="blue", marker='none', label='fit CDF')

    plt.xlabel(xlabel)
    plt.ylabel("probability")
    if failure_window:
        failure_pdf = failure_prob(fit_dist, x_to_plot, failure_window)
        max_i, max_val = np.argmax(failure_pdf), np.max(failure_pdf)
        max_penalty = failure_penalty(max_val, failure_window)
        print "Max failure prob %f (penalty %f) at time %f" % (max_val, max_penalty, x[max_i])
        plt.plot(x_to_plot, failure_pdf, color="red", marker='none', 
                 label="%f-second\nfailure PDF" % failure_window)

        plt.legend(loc=4)

        penalties = failure_penalty(failure_pdf, failure_window)
        twin = plt.twinx()
        twin.set_ylabel("Failure penalty (sec)")
        twin.plot(x_to_plot, penalties, color="purple", linestyle="-", marker="none",
                  label="failure penalty")
        plt.legend(loc=4)

    fit_pdf_values_to_plot = fit_dist.pdf(x_to_plot)

    plt.figure()
    plt.plot(x_to_plot, fit_pdf_values_to_plot, label="fit PDF")
    

def generate_weibull_samples(num_samples=100, params=None):
    fixed_shape = 2.0
    fixed_loc = 0.0
    fixed_scale = 4.0
    if params:
        fixed_shape, fixed_scale = params

    print "actual: (shape=%f loc=%f scale=%f)" % (fixed_shape, fixed_loc, fixed_scale)
    weibull = stats.weibull_min(fixed_shape, loc=fixed_loc, scale=fixed_scale)
    return generate_samples(weibull, num_samples)

def generate_samples(distribution, num_samples):
    samples = distribution.rvs(num_samples)
    if max_value:
        x = cdf_xvalues(max_value)
    else:
        x = cdf_xvalues(max(samples))
    cdf_values = distribution.cdf(x)

    plt.plot(x, cdf_values, color="green", marker='none', label='actual CDF')
    return samples

def plot_weibull_ccdf(shape, scale, max_value):
    weibull = stats.weibull_min(shape, loc=0.0, scale=scale)
    print ("Weibull: shape %f scale %f mean %f median %f" 
           % (shape, scale, weibull.mean(), weibull.median()))
    return plot_ccdf(weibull)

def plot_ccdf(distribution):
    x = cdf_xvalues(max_value)
    ccdf_values = 1 - distribution.cdf(x)
    plt.loglog(x, ccdf_values)
    

def main():
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--gen-samples", type=int, metavar="NUM_SAMPLES")
    parser.add_argument("--params", type=float, nargs=2, metavar=['SHAPE', 'SCALE'])
    parser.add_argument("--stdin", action="store_true", default=False)
    parser.add_argument("--failure-window", type=float)
    parser.add_argument("--kaist-paper-params", action="store_true", default=False)
    parser.add_argument("--max-value", type=float)
    parser.add_argument("--xlabel", default="value")
    args = parser.parse_args()
    
    if args.max_value:
        global max_value
        max_value = args.max_value

    if args.gen_samples:
        samples = generate_weibull_samples(args.gen_samples, args.params)
    elif args.stdin:
        samples = [float(x.strip()) for x in sys.stdin.readlines()]
        print samples
    elif args.kaist_paper_params:
        # None of these seem to match the CCDF in the paper.
        plot_weibull_ccdf(0.52, (6.17 * np.e - 4), 100)
        plt.figure()
        plot_weibull_ccdf(0.52, (6.17 * (np.e ** -4)), 100)
        plt.figure()
        plot_weibull_ccdf(0.52, (6.17 * (10 ** -4)), 100)
        plt.figure()

        samples = generate_weibull_samples(100000, params=[0.52, (6.17 * np.e - 4) * 3600])
        
    else:
        samples = generate_weibull_samples(params=args.params)
        
    plot_fit(samples, args.failure_window, xlabel=args.xlabel)
    plt.show()

if __name__ == '__main__':
    main()
