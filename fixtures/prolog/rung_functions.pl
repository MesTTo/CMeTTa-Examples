% Two predicates in the engine's function-form convention: the last argument
% is the answer, so `import_prolog_functions` can register them as MeTTa
% functions. Loaded by 05-the-module-doors.metta through four different doors,
% which is what that file is about.
rung_double(X, Y) :- Y is X * 2.
rung_greeting(world).
