### Filter using direct text matching
**Before**:

`catlr react/app/ -e modules` excludes `node_modules`, `my_modules`, and any file containing the word "modules".

**Desired**:
`catlr react/app/ -e modules` should only 