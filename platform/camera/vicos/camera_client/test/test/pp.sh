for f in *.rgb
do
 echo "Processing $f"
 convert -size 640x360 -depth 8 $f  ${f}.png    
 # && open ${f}.png
done
convert  -loop 0 *.png animation.gif
rm *.png *.rgb
open animation.gif
