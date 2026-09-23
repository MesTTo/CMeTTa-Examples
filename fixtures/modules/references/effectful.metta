; Purpose: prove non-eager reference admission refuses an effectful initializer.
; [tested: examples/ch20-extending-the-engine/20-04-modules-and-the-catalog/13-reference_loading.metta; commit=90ba93eb8f6e98ebfefc55416859bf13de6a8427].
!(println! forbidden-initializer)
(= (effectful-value) 1)
