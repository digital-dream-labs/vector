# import the necessary packages
from imutils import build_montages
from imutils import paths
import numpy as np
import argparse
import imutils
import cv2


# Compute colorfulness based on Hasler and Süsstrunk:
# https://infoscience.epfl.ch/record/33994/files/HaslerS03.pdf
# https://www.pyimagesearch.com/2017/06/05/computing-image-colorfulness-with-opencv-and-python/
def colorful_score(image):
	# split the image into its respective RGB components
	(B, G, R) = cv2.split(image.astype("float"))
 
	# compute rg = R - G
	rg = np.absolute(R - G)
 
	# compute yb = 0.5 * (R + G) - B
    # could be optimized by doing a >> 1 ?
	yb = np.absolute(0.5 * (R + G) - B)
 
	# compute the mean and standard deviation of both `rg` and `yb`
	(rbMean, rbStd) = (np.mean(rg), np.std(rg))
	(ybMean, ybStd) = (np.mean(yb), np.std(yb))
 
	# combine the mean and standard deviations
	stdRoot = np.sqrt((rbStd ** 2) + (ybStd ** 2))
	meanRoot = np.sqrt((rbMean ** 2) + (ybMean ** 2))
 
	# derive the "colorfulness" metric and return it
	return stdRoot + (0.3 * meanRoot)


############### MAIN PROGRAM ######################

# Number of x by y tiles we divide the image into
# It's the 'resolution' of the color detector
nrxytiles = 5
# How wide to we want the single tiles to be?
tilew = 96

#Load a test image
image = cv2.imread("colors07.jpg")
#image = cv2.imread("photo_000015.jpg")
image = imutils.resize(image, width = tilew * nrxytiles)
#make a copy to draw on
drawimg = image

#Compute the tile sizes in pixels
height, width, channels = image.shape
#tilew = tilew 
tileh = int(height / nrxytiles)


i = 0
for h in range(0, height-tileh+1, tileh):
    for w in range(0, width-tilew+1, tilew):
        
        i = i +1

        # get the current tile
        tile = image[h:h+tileh, w:w+tilew]
        score = colorful_score(tile)
        
        #this is just drawing stuff
        cv2.rectangle(drawimg, (w, h), (w+width, h+height), (200,200,200), 1)
        font = cv2.FONT_HERSHEY_SIMPLEX
        cv2.putText(drawimg, str(int(round(score))), (w+5,h+10), font, 0.4, (0, 255, 0))
        
        print (i, score)


#draw the image
cv2.imshow("result", drawimg)
k = cv2.waitKey(0)