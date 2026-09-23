% A foreign space provider, complete and deliberately small, for
% examples/ch08-data/08-03-the-shipped-libraries/09-conformance.metta to prove.
%
% It declares an EXTENSION and exports nothing, which is the shape of a
% provider-only file: metta_export is for functions and a provider has none.
% spaces([...]) is the readying moment: at load-end the engine confirms the
% space is registered, declares capabilities, and has a clause behind every
% declared hook.
:- metta_extension(demo_provider, [version('1.0.0'), spaces(['&demo_provider'])]).

:- multifile seam:foreign_space/1.
:- multifile seam:foreign_atoms/2.
:- multifile seam:foreign_match/3.
:- multifile seam:foreign_capability/2.

demo_edge([edge, a, b]).
demo_edge([edge, b, c]).

seam:foreign_space('&demo_provider').
seam:foreign_atoms('&demo_provider', Atom) :- demo_edge(Atom).

% Over-approximating: every atom, every time. Always correct, because the
% engine keeps unification.
seam:foreign_match('&demo_provider', Pattern, _Options) :- demo_edge(Pattern).

% Exactly what it provides, so the engine's own record is the truth.
seam:foreign_capability('&demo_provider', Capability) :-
    member(Capability, [match, enumerate]).
