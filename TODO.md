### Filter using direct text matching
**NOTE:** every text matching is case sensitive

**Before**:
- `catlr react/app/ -e modules` excludes `node_modules`, `my_modules`, and any file containing the word "modules".

**Desired**:
- `catlr react/app/ -e modules` should only exclude the file `react/app/modules`
- `catlr react/app/ -e modules/` should exclude (recursively) the dir `react/app/modules/`
- `catlr react/app/ -e *modules*` should then exclude everything, each directory and file that has modules in its name (e.g. `node_modules/`, `modules.txt`, etc.)

### Filtering flags allow permutation
#### Filtering flags are:
- `-i`: Include file or folder (has priority over `-e`)
- `-e`: Exclude file or folder

- `-p`: Filter modifier, apply filter to printing stage
- `-l`: Filter modifier, apply filter to listing stage

#### Thus, the permutations are:
- `-e == -pel x == -epl == -el...`: Exclude `x` from printing and listing. (So `x` wont apper on the autid at all).

- `-i == -pil x == -ipl == -il...`: Include `x` to printing and listing. 

Include is implicit, so only makes sense to use the -i flag if it has be exxcluded before, for example:
- `-e node_modules/ -il node_modules/lucide_icons/`: Excludes from printing and listing the whole `node_modules` dir, but includes the `node_moodules/lucide_icons` dir into the tree listing (doesnt print its files' contents, just the structure).