#! /usr/bin/env python

# import default python libraries
import numpy as np
import os
from functions import *

# Run a simple threshold filter and write out mask:

def filter(mask, cube, header, clipMethod, threshold, rmsMode, verbose):
	if clipMethod == 'relative':
		# Determine the clip level
		# Measure noise in original cube
		rms = GetRMS(cube, rmsMode=rmsMode, zoomx=1, zoomy=1, zoomz=1, verbose=verbose)
		print 'Estimated rms = ', rms
		clip = threshold * rms
	if clipMethod == 'absolute':
		clip = threshold
	print 'Using clip threshold: ', clip

	# Check whether there are NaNs:
	nan_mask = np.isnan(cube)
	found_nan = nan_mask.sum()
	
	# Set Nans to zero (and INFs to a finite number) if necessary:
	if found_nan: cube = np.nan_to_num(cube)
	
	# Run the threshold finder, setting bit 1 of the mask for positive signals...
	np.bitwise_or(mask, np.greater_equal(cube, clip), mask)
	# ...and bit 2 for negative signals:
	np.bitwise_or(mask, np.less_equal(cube, -clip) * 0x02, mask)
	
	# Put NaNs (but not INFs) back into the data cube:
	if found_nan: cube[nan_mask] = np.nan
	
	return
