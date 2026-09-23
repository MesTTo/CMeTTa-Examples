% Purpose: build a throwaway local git repository holding one MeTTa library,
%   so the git-import! example acquires a real repository without reaching a
%   network. The suite ran on every push and cloned github each time.
% Assumes:
%   - git is on PATH, which git-import! already requires
%     [source: lib/lib_gitimport/lib_gitimport.pl, git_process/5]
% Guarantees:
%   - the path it answers is a git repository whose HEAD holds fixture.metta,
%     and no earlier checkout of it survives the call
%     [tested: examples/ch20-extending-the-engine/20-04-modules-and-the-catalog/06-git_import.metta]
% Owns: <Base>/.sources/metta_fixture_lib and <Base>/metta_fixture_lib, both
%   deleted and rebuilt per call. Nothing else may keep files there.
% Open Obligations:
%   To Do: None
%   Hacks: None
%   Future Enhancements: None

:- use_module(library(filesex)).
:- use_module(library(process)).

git_fixture_name(metta_fixture_lib).

%Function-form convention, so MeTTa imports this as a one-argument function:
%the base directory goes in, the clone URL comes out.
%
%Both the source and any checkout of it are rebuilt, because git-import!
%does not fetch into a checkout that already exists: leaving one behind
%would silently serve a stale library after this fixture changed.
git_fixture_url(Base0, Url) :-
    git_fixture_atom(Base0, Base),
    git_fixture_name(Name),
    directory_file_path(Base, '.sources', SourceRoot),
    directory_file_path(SourceRoot, Name, SourceDir),
    directory_file_path(Base, Name, CloneDir),
    forall(member(Stale, [SourceDir, CloneDir]),
           ( exists_directory(Stale) -> delete_directory_and_contents(Stale)
           ; true )),
    make_directory_path(SourceDir),
    git_fixture_write_library(SourceDir),
    git_fixture_commit(SourceDir),
    absolute_file_name(SourceDir, Url, [file_type(directory)]).

git_fixture_atom(Value, Atom) :- atom(Value), !, Atom = Value.
git_fixture_atom(Value, Atom) :- atom_string(Atom, Value).

git_fixture_write_library(Dir) :-
    directory_file_path(Dir, 'fixture.metta', Library),
    setup_call_cleanup(
        open(Library, write, Stream),
        format(Stream, "(= (fixture-answer $x) (* $x 3))~n", []),
        close(Stream)).

%An explicit identity, because a machine with no git user configured would
%otherwise fail the commit rather than the test it is meant to support.
git_fixture_commit(Dir) :-
    git_fixture_run(Dir, [init, '-q']),
    git_fixture_run(Dir, [add, 'fixture.metta']),
    git_fixture_run(Dir, ['-c', 'user.email=fixture@metta.invalid',
                          '-c', 'user.name=metta fixture',
                          commit, '-q', '-m', 'fixture library']).

git_fixture_run(Dir, Arguments) :-
    setup_call_cleanup(
        process_create(path(git), Arguments,
                       [cwd(Dir), stdout(null), stderr(pipe(Error)),
                        process(PID)]),
        read_string(Error, _, Diagnostic),
        close(Error)),
    process_wait(PID, Status),
    ( Status == exit(0)
      -> true
    ; throw(error(metta_git_fixture_failed(Arguments, Status, Diagnostic),
                  context(git_fixture_run/2,
                          'could not build the local git fixture'))) ).
