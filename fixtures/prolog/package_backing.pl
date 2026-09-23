% The artifact a package row names, for 06-package_rows.metta.
%
% A clause of arity N+1 is a MeTTa function of arity N and its last argument is
% the answer, which is the same convention import_prolog_functions_from_file
% uses in 02-prologimport.metta. Two heads rather than one, so the example can
% show that a row registers exactly the names it lists and no others: this file
% also exports `unlisted`, which the row leaves alone.
greeting('hello').

doubled(Number, Twice) :- Twice is Number * 2.

unlisted('not registered by the row').
