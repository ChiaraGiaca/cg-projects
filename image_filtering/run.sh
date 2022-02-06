./bin/ycolorgrade tests/greg_zaal_artist_workshop.hdr -e 0 -o out/greg_zaal_artist_workshop_01.jpg
./bin/ycolorgrade tests/greg_zaal_artist_workshop.hdr -e 1 -f -c 0.75 -s 0.75 -o out/greg_zaal_artist_workshop_02.jpg
./bin/ycolorgrade tests/greg_zaal_artist_workshop.hdr -e 0.8 -c 0.6 -s 0.5 -g 0.5 -o out/greg_zaal_artist_workshop_03.jpg
./bin/ycolorgrade tests/greg_zaal_artist_workshop.hdr -seppia -o out/greg_zaal_artist_workshop_04.jpg
./bin/ycolorgrade tests/greg_zaal_artist_workshop.hdr -e 0.50 -c 0.5 -s 1.2 -sun -o out/greg_zaal_artist_workshop_05.jpg

./bin/ycolorgrade tests/toa_heftiba_people.jpg -e -1 -f -c 0.75 -s 0.3 -v 0.4 -o out/toa_heftiba_people_01.jpg
./bin/ycolorgrade tests/toa_heftiba_people.jpg -e -0.5 -c 0.75 -s 0 -o out/toa_heftiba_people_02.jpg
./bin/ycolorgrade tests/toa_heftiba_people.jpg -e -0.5 -c 0.6 -s 0.7 -tr 0.995 -tg 0.946 -tb 0.829 -g 0.3 -o out/toa_heftiba_people_03.jpg
./bin/ycolorgrade tests/toa_heftiba_people.jpg -m 16 -G 16 -o out/toa_heftiba_people_04.jpg
./bin/ycolorgrade tests/toa_heftiba_people.jpg -c 0.6 -s 0 -eff -o out/toa_heftiba_people_05.jpg	
./bin/ycolorgrade tests/toa_heftiba_people.jpg -c 0.60 -s 0.3 -tr 1.1 -tg 0.985 -tb 0.999 -vin -o out/toa_heftiba_people_06.jpg	


./bin/ycolorgrade tests/inside_out.jpg -red -o out/inside_out.jpg
./bin/ycolorgrade tests/red_white_roses_wedding_flowers_package_auckland_delivery.jpg -red -o out/red_white_roses_wedding_flowers_package_auckland_delivery.jpg
