
# ### Copy folders
# scp -pr pano_template_2.0 high_wall_mining_collom_pit_colowyo_mine
#
# ### Convert and copy image
# convert PANOS/0B5A1942_high_wall_mining_collom_pit_colowyo_mine_pano.jpg -resize 8192 high_wall_mining_collom_pit_colowyo_mine/data/high_wall_mining_collom_pit_colowyo_mine_1942_8K.jpg
#
# ### Rename files
# mv high_wall_mining_collom_pit_colowyo_mine/pano_template.asset                           high_wall_mining_collom_pit_colowyo_mine/high_wall_mining_collom_pit_colowyo_mine.asset
# mv high_wall_mining_collom_pit_colowyo_mine/pano_template_360.asset                       high_wall_mining_collom_pit_colowyo_mine/high_wall_mining_collom_pit_colowyo_mine_360.asset
# mv high_wall_mining_collom_pit_colowyo_mine/pano_template_models.asset                    high_wall_mining_collom_pit_colowyo_mine/high_wall_mining_collom_pit_colowyo_mine_models.asset
# mv high_wall_mining_collom_pit_colowyo_mine/pano_template_transforms.asset                high_wall_mining_collom_pit_colowyo_mine/high_wall_mining_collom_pit_colowyo_mine_transforms.asset
#
# ### Set image name
# perl -pi -e "s/pano_template.jpg/high_wall_mining_collom_pit_colowyo_mine_1942_8K.jpg/"                high_wall_mining_collom_pit_colowyo_mine/*360.asset
#
# ### Swap names
# perl -pi -e "s/pano_template/high_wall_mining_collom_pit_colowyo_mine/"                high_wall_mining_collom_pit_colowyo_mine/*.asset
# perl -pi -e "s/pano_template/high_wall_mining_collom_pit_colowyo_mine/"                high_wall_mining_collom_pit_colowyo_mine/*.asset
# perl -pi -e "s/pano_template/high_wall_mining_collom_pit_colowyo_mine/"                high_wall_mining_collom_pit_colowyo_mine/*.asset
#
# perl -pi -e "s/Latitude = LATITUDE_VALUE/Latitude = 40.27031667/" high_wall_mining_collom_pit_colowyo_mine/*transforms.asset
# perl -pi -e "s/Longitude = LONGITUDE_VALUE/Longitude = -107.9099222/" high_wall_mining_collom_pit_colowyo_mine/*transforms.asset
# ### Set up rotation of image
# perl -pi -e"s/Rotation = {0.00,0.69,-1.77}/Rotation = {0.00,0.702849616654087,-1.883383438348777}/" high_wall_mining_collom_pit_colowyo_mine/*360.asset


### Copy folders
scp -pr pano_template_0.20 tristate_control_room_pano_1/
scp -pr pano_template_0.20 tristate_control_room_pano_2/
scp -pr pano_template_0.20 tristate_erie_substation_pano_1/
scp -pr pano_template_0.20 tristate_erie_substation_pano_2/
scp -pr pano_template_0.20 tristate_erie_substation_pano_3/
scp -pr pano_template_0.20 tristate_erie_substation_pano_4/
scp -pr pano_template_0.20 tristate_erie_substation_pano_5/

### Convert and copy image
convert PANO_IMAGES/tri-state_control_room_pano_1_edit.jpg             -resize 8192 tristate_control_room_pano_1/data/tristate_control_room_pano_1_8K.jpg
convert PANO_IMAGES/0B5A1358_tri-state_control_room_pano_2_edit.jpg    -resize 8192 tristate_control_room_pano_2/data/tristate_control_room_pano_2_8K.jpg
convert PANO_IMAGES/0B5A1373_tri-state_erie_substation_pano_1_edit.jpg -resize 8192 tristate_erie_substation_pano_1/data/tristate_erie_substation_pano_1_8K.jpg
convert PANO_IMAGES/0B5A1391_tri-state_erie_substation_pano_2_edit.jpg -resize 8192 tristate_erie_substation_pano_2/data/tristate_erie_substation_pano_2_8K.jpg
convert PANO_IMAGES/0B5A1405_erie_substation_pano_3_edit.jpg           -resize 8192 tristate_erie_substation_pano_3/data/tristate_erie_substation_pano_3_8K.jpg
convert PANO_IMAGES/0B5A1405_tri-state_erie_substation_pano_4_edit.jpg -resize 8192 tristate_erie_substation_pano_4/data/tristate_erie_substation_pano_4_8K.jpg
convert PANO_IMAGES/0B5A1433_tri-state_erie_substation_pano_5_edit.jpg -resize 8192 tristate_erie_substation_pano_5/data/tristate_erie_substation_pano_5_8K.jpg

### Rename files
mv tristate_control_room_pano_1/pano_template.asset                             tristate_control_room_pano_1/tristate_control_room_pano_1.asset
mv tristate_control_room_pano_1/pano_template_360.asset                         tristate_control_room_pano_1/tristate_control_room_pano_1_360.asset
mv tristate_control_room_pano_1/pano_template_models.asset                      tristate_control_room_pano_1/tristate_control_room_pano_1_models.asset
mv tristate_control_room_pano_1/pano_template_transforms.asset                  tristate_control_room_pano_1/tristate_control_room_pano_1_transforms.asset

mv tristate_control_room_pano_2/pano_template.asset                             tristate_control_room_pano_2/tristate_control_room_pano_2.asset
mv tristate_control_room_pano_2/pano_template_360.asset                         tristate_control_room_pano_2/tristate_control_room_pano_2_360.asset
mv tristate_control_room_pano_2/pano_template_models.asset                      tristate_control_room_pano_2/tristate_control_room_pano_2_models.asset
mv tristate_control_room_pano_2/pano_template_transforms.asset                  tristate_control_room_pano_2/tristate_control_room_pano_2_transforms.asset

mv tristate_erie_substation_pano_1/pano_template.asset                          tristate_erie_substation_pano_1/tristate_erie_substation_pano_1.asset
mv tristate_erie_substation_pano_1/pano_template_360.asset                      tristate_erie_substation_pano_1/tristate_erie_substation_pano_1_360.asset
mv tristate_erie_substation_pano_1/pano_template_models.asset                   tristate_erie_substation_pano_1/tristate_erie_substation_pano_1_models.asset
mv tristate_erie_substation_pano_1/pano_template_transforms.asset               tristate_erie_substation_pano_1/tristate_erie_substation_pano_1_transforms.asset

mv tristate_erie_substation_pano_2/pano_template.asset                          tristate_erie_substation_pano_2/tristate_erie_substation_pano_2.asset
mv tristate_erie_substation_pano_2/pano_template_360.asset                      tristate_erie_substation_pano_2/tristate_erie_substation_pano_2_360.asset
mv tristate_erie_substation_pano_2/pano_template_models.asset                   tristate_erie_substation_pano_2/tristate_erie_substation_pano_2_models.asset
mv tristate_erie_substation_pano_2/pano_template_transforms.asset               tristate_erie_substation_pano_2/tristate_erie_substation_pano_2_transforms.asset

mv tristate_erie_substation_pano_3/pano_template.asset                          tristate_erie_substation_pano_3/tristate_erie_substation_pano_3.asset
mv tristate_erie_substation_pano_3/pano_template_360.asset                      tristate_erie_substation_pano_3/tristate_erie_substation_pano_3_360.asset
mv tristate_erie_substation_pano_3/pano_template_models.asset                   tristate_erie_substation_pano_3/tristate_erie_substation_pano_3_models.asset
mv tristate_erie_substation_pano_3/pano_template_transforms.asset               tristate_erie_substation_pano_3/tristate_erie_substation_pano_3_transforms.asset

mv tristate_erie_substation_pano_4/pano_template.asset                          tristate_erie_substation_pano_4/tristate_erie_substation_pano_4.asset
mv tristate_erie_substation_pano_4/pano_template_360.asset                      tristate_erie_substation_pano_4/tristate_erie_substation_pano_4_360.asset
mv tristate_erie_substation_pano_4/pano_template_models.asset                   tristate_erie_substation_pano_4/tristate_erie_substation_pano_4_models.asset
mv tristate_erie_substation_pano_4/pano_template_transforms.asset               tristate_erie_substation_pano_4/tristate_erie_substation_pano_4_transforms.asset

mv tristate_erie_substation_pano_5/pano_template.asset                          tristate_erie_substation_pano_5/tristate_erie_substation_pano_5.asset
mv tristate_erie_substation_pano_5/pano_template_360.asset                      tristate_erie_substation_pano_5/tristate_erie_substation_pano_5_360.asset
mv tristate_erie_substation_pano_5/pano_template_models.asset                   tristate_erie_substation_pano_5/tristate_erie_substation_pano_5_models.asset
mv tristate_erie_substation_pano_5/pano_template_transforms.asset               tristate_erie_substation_pano_5/tristate_erie_substation_pano_5_transforms.asset

### Set image name
perl -pi -e "s/pano_template.jpg/tristate_control_room_pano_1_8K.jpg/"          tristate_control_room_pano_1/*360.asset
perl -pi -e "s/pano_template.jpg/tristate_control_room_pano_2_8K.jpg/"          tristate_control_room_pano_2/*360.asset
perl -pi -e "s/pano_template.jpg/tristate_erie_substation_pano_1_8K.jpg/"       tristate_erie_substation_pano_1/*360.asset
perl -pi -e "s/pano_template.jpg/tristate_erie_substation_pano_2_8K.jpg/"       tristate_erie_substation_pano_2/*360.asset
perl -pi -e "s/pano_template.jpg/tristate_erie_substation_pano_3_8K.jpg/"       tristate_erie_substation_pano_3/*360.asset
perl -pi -e "s/pano_template.jpg/tristate_erie_substation_pano_4_8K.jpg/"       tristate_erie_substation_pano_4/*360.asset
perl -pi -e "s/pano_template.jpg/tristate_erie_substation_pano_5_8K.jpg/"       tristate_erie_substation_pano_5/*360.asset

### Swap names
perl -pi -e "s/pano_template/tristate_control_room_pano_1/"                     tristate_control_room_pano_1/*.asset
perl -pi -e "s/pano_template/tristate_control_room_pano_2/"                     tristate_control_room_pano_2/*.asset
perl -pi -e "s/pano_template/tristate_erie_substation_pano_1/"                  tristate_erie_substation_pano_1/*.asset
perl -pi -e "s/pano_template/tristate_erie_substation_pano_2/"                  tristate_erie_substation_pano_2/*.asset
perl -pi -e "s/pano_template/tristate_erie_substation_pano_3/"                  tristate_erie_substation_pano_3/*.asset
perl -pi -e "s/pano_template/tristate_erie_substation_pano_4/"                  tristate_erie_substation_pano_4/*.asset
perl -pi -e "s/pano_template/tristate_erie_substation_pano_5/"                  tristate_erie_substation_pano_5/*.asset

perl -pi -e "s/PANO TEMPLATE/Tri-State Control Room Pano 1/"                    tristate_control_room_pano_1/*.asset
perl -pi -e "s/PANO TEMPLATE/Tri-State Control Room Pano 2/"                    tristate_control_room_pano_2/*.asset
perl -pi -e "s/PANO TEMPLATE/Tri-State Erie Substation Pano 1/"                 tristate_erie_substation_pano_1/*.asset
perl -pi -e "s/PANO TEMPLATE/Tri-State Erie Substation Pano 2/"                 tristate_erie_substation_pano_2/*.asset
perl -pi -e "s/PANO TEMPLATE/Tri-State Erie Substation Pano 3/"                 tristate_erie_substation_pano_3/*.asset
perl -pi -e "s/PANO TEMPLATE/Tri-State Erie Substation Pano 4/"                 tristate_erie_substation_pano_4/*.asset
perl -pi -e "s/PANO TEMPLATE/Tri-State Erie Substation Pano 5/"                 tristate_erie_substation_pano_5/*.asset

### Set coordinates
perl -pi -e "s/Latitude = LATITUDE_VALUE/Latitude = 39.9064861111/"             tristate_control_room_pano_1/*transforms.asset
perl -pi -e "s/Longitude = LONGITUDE_VALUE/Longitude = -105.0001833333/"        tristate_control_room_pano_1/*transforms.asset
perl -pi -e "s/P_ANGLE/0.6964995755/"                                           tristate_control_room_pano_1/*360.asset
perl -pi -e "s/R_ANGLE/-1.8325989144/"                                          tristate_control_room_pano_1/*360.asset

perl -pi -e "s/Latitude = LATITUDE_VALUE/Latitude = 39.9064861111/"             tristate_control_room_pano_2/*transforms.asset
perl -pi -e "s/Longitude = LONGITUDE_VALUE/Longitude = -105.0001833333/"        tristate_control_room_pano_2/*transforms.asset
perl -pi -e "s/P_ANGLE/0.6964995755/"                                           tristate_control_room_pano_2/*360.asset
perl -pi -e "s/R_ANGLE/-1.8325989144/"                                          tristate_control_room_pano_2/*360.asset

perl -pi -e "s/Latitude = LATITUDE_VALUE/Latitude = 40.0273472222/"             tristate_erie_substation_pano_1/*transforms.asset
perl -pi -e "s/Longitude = LONGITUDE_VALUE/Longitude = -104.9567333333/"        tristate_erie_substation_pano_1/*transforms.asset
perl -pi -e "s/P_ANGLE/0.6986089999/"                                           tristate_erie_substation_pano_1/*360.asset
perl -pi -e "s/R_ANGLE/-1.8318405688/"                                          tristate_erie_substation_pano_1/*360.asset

perl -pi -e "s/Latitude = LATITUDE_VALUE/Latitude = 40.0275388889/"             tristate_erie_substation_pano_2/*transforms.asset
perl -pi -e "s/Longitude = LONGITUDE_VALUE/Longitude = -104.9571777778/"        tristate_erie_substation_pano_2/*transforms.asset
perl -pi -e "s/P_ANGLE/0.6986123451/"                                           tristate_erie_substation_pano_2/*360.asset
perl -pi -e "s/R_ANGLE/-1.8318483258/"                                          tristate_erie_substation_pano_2/*360.asset

perl -pi -e "s/Latitude = LATITUDE_VALUE/Latitude = 40.0271638889/"             tristate_erie_substation_pano_3/*transforms.asset
perl -pi -e "s/Longitude = LONGITUDE_VALUE/Longitude = -104.9571944444/"        tristate_erie_substation_pano_3/*transforms.asset
perl -pi -e "s/P_ANGLE/0.6986058001/"                                           tristate_erie_substation_pano_3/*360.asset
perl -pi -e "s/R_ANGLE/-1.8318486167/"                                          tristate_erie_substation_pano_3/*360.asset

perl -pi -e "s/Latitude = LATITUDE_VALUE/Latitude = 40.0269777778/"             tristate_erie_substation_pano_4/*transforms.asset
perl -pi -e "s/Longitude = LONGITUDE_VALUE/Longitude = -104.9561750000/"        tristate_erie_substation_pano_4/*transforms.asset
perl -pi -e "s/P_ANGLE/0.6986025518/"                                           tristate_erie_substation_pano_4/*360.asset
perl -pi -e "s/R_ANGLE/-1.8318308240/"                                          tristate_erie_substation_pano_4/*360.asset

perl -pi -e "s/Latitude = LATITUDE_VALUE/Latitude = 40.0268250000/"             tristate_erie_substation_pano_5/*transforms.asset
perl -pi -e "s/Longitude = LONGITUDE_VALUE/Longitude = -104.9559861111/"        tristate_erie_substation_pano_5/*transforms.asset
perl -pi -e "s/P_ANGLE/0.6985998854/"                                           tristate_erie_substation_pano_5/*360.asset
perl -pi -e "s/R_ANGLE/-1.8318275273/"                                          tristate_erie_substation_pano_5/*360.asset

