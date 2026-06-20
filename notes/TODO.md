# TODO
## Tasks
- [ ] Fixing GDAL bug that causes layers to disappear or not load after OpenSpace has been running for awhile
  - Related: [`#3869`](https://github.com/OpenSpace/OpenSpace/issues/3869) Temporal layer caches unavailable data; [`#386`](https://github.com/OpenSpace/OpenSpace/issues/386) Make temporal tile layers independent of GDAL; [`#3031`](https://github.com/OpenSpace/OpenSpace/issues/3031) Earth missing tiles on startup (closed)
- [ ] Debug the rendering of geospatial image layers that that are being loaded as .vrt (ViRTual dataset) files
  - Related: [`#1243`](https://github.com/OpenSpace/OpenSpace/issues/1243) Understand NoData values to not use the Alpha channel; [`#1588`](https://github.com/OpenSpace/OpenSpace/issues/1588) VRT layers are not cached with WMSCacheEnabled (closed)
- [ ] Making rendering of GeoJSONs more efficient by loading only those primitives that are visible [`#2730`](https://github.com/OpenSpace/OpenSpace/issues/2730) Improve performance of GeoJson rendering
- [ ] Adding ability to layer animations over part of the Earth in addition to over the entire globe / Create the ability to geospatially place movies onto the surfaces of planets
  - Related: [`#3846`](https://github.com/OpenSpace/OpenSpace/issues/3846) Mismatching video / NOAA SOS layer on Earth; [`#2683`](https://github.com/OpenSpace/OpenSpace/issues/2683) Video player on globe rendering flickering (closed); [`#2684`](https://github.com/OpenSpace/OpenSpace/issues/2684) Add video layer to Jupiter for Juno profile (closed)
- [ ] Overlaying geospatial animated maps onto Earth’s surface, there are other example assets I can send you at a future date.
  - Related: [`#4025`](https://github.com/OpenSpace/OpenSpace/issues/4025) Globe Imagery Browser should add Temporal layers when possible; [`#2830`](https://github.com/OpenSpace/OpenSpace/issues/2830) Globebrowsing support scripts for NOAA Temporal datasets via GDAL
- [ ] overlaying geospatial animated maps onto Earth’s surface, there are other examples
  - Related: [`#3846`](https://github.com/OpenSpace/OpenSpace/issues/3846) Mismatching video / NOAA SOS layer on Earth; [`#4025`](https://github.com/OpenSpace/OpenSpace/issues/4025) Globe Imagery Browser should add Temporal layers when possible (duplicate of the task above)
- [ ] Finish postprocessing pipeline work
  - Related: [`#3828`](https://github.com/OpenSpace/OpenSpace/issues/3828) Robust Postprocessing Support; [`#249`](https://github.com/OpenSpace/OpenSpace/issues/249) Add post-processing operation to change brightness/contrast (closed)\
## Notes
 Finally there are a bunch of GDAL layers that can be loaded and toggled on/off in Row 8. These are the ones that might load at the start and render, but after a while, they stop showing up. (Or they partially render; for these geospatial maps, I tend to see the layers show up in the northeastern corner of the US, while staying invisible elsewhere.)
    - You’ll find in the zip file a user/data/… and user/recordings/ folder structure that are copied into the user/ folder. There’s also a HTML_BROWSER_CONTROLS/ folder that contains the HTML page for loading and controlling the presentation. If you click through the top row of LOAD and SETUP buttons in order, you’ll load and configure the geoJSONs. (You’ll see them flash on and off in OpenSpace as each configuration loop runs.)  
## Resources
- [data GeoJson](https://www.dropbox.com/scl/fi/uv7rd7lto4itlmtz6y6ty/DIGITAL_EARTH_ENERGY.zip?rlkey=l905fa217yzzgbgkhc31azskv&st=gkk72qv1&dl=0)

For the third task of overlaying geospatial animated maps onto Earth’s surface, there are other example assets I can send you at a future date.

You’ll find in the zip file a user/data/… and user/recordings/ folder structure that are copied into the user/ folder. There’s also a HTML_BROWSER_CONTROLS/ folder that contains the HTML page for loading and controlling the presentation. If you click through the top row of LOAD and SETUP buttons in order, you’ll load and configure the geoJSONs. (You’ll see them flash on and off in OpenSpace as each configuration loop runs.)  

The geoJSONs that can slow OpenSpace to a crawl are in the buttons in rows 6 and 7. In row 6, the “CO: Public Service”, “CO: Tri-State”, and “CO: WAPA” buttons toggle on/off electrical grid lines. Row 7 loads additional transmission grid line files that are so large that I kept the LOAD and SETUP buttons separate on this row.

Finally there are a bunch of GDAL layers that can be loaded and toggled on/off in Row 8. These are the ones that might load at the start and render, but after a while, they stop showing up. (Or they partially render; for these geospatial maps, I tend to see the layers show up in the northeastern corner of the US, while staying invisible elsewhere.)

