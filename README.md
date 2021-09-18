# vector

Dynamic array (aka vector) with small-object and copy-on-write optimizations. 
The project includes C++ optimized vector library, unit tests, integration tests and exception safety tests.

## Implementation requirements

* __Small-object optimization__: when the vector is small enough, its data should be kept in a static buffer. This optimization helps to avoid too many memory reallocations for the small arrays.
* __Copy-on-write optimization__: when the vector is copied the copy should only store the link to the original array and allocate more memory only during the update of its content.
* The amount of memory allowed for the one vector's instance is calculated by the formula: $sizeof(vector<T>) \leq max(2*sizeof(void*), sizeof(void*) + sizeof(T))$
* It's restricted to allocate more than one piece of memory at the same time.
* It's possible that $T$ doesn't have a default constructor. 
* __Exception guarantees__: 
	* $base$ for $insert$ and $erase$
	* $noexcept$ for all the $const$ methods
	* $strong$ otherwise
