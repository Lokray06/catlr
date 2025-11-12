catlr /some/dir -i node_modules
#Excludes /some/dir/node_modules whole dir and files (so it skips it when recursing), both for the listing and the printing

catlr /some/dir -pi node_modules or catlr /some/dir -ip node_modules
#Excludes /some/dir/node_modules whole dir and files (so it skips it when recursing), only for the printing, so it also lists it in the tree

#and ofc the same the other way

catlr /some/dir -le node_modules or catlr /some/dir -el node_modules
#Excludes /some/dir/node_modules whole dir and files (so it skips it when recursing), only for the listing, so it also prints everything, just doesnt print

catlr /some/dir -lpe node_modules or catlr /some/dir -pel node_modules or ... more permutations == catlr /some/dir -e node_modules
#This is the same as teh first example, but is kinda stupid

#And the same for files
catlr /my/app/with/secrets -e .env
#Exludes env files

catlr /my/downloads/ -e personalFolder -i personalFolder/exceptionFolder
# SOme exapmles of combinations of both