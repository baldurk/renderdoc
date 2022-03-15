# Developing a change

If you're making a tiny bugfix that only changes a few lines, then you don't really have to worry about how you structure your development or organise branches. You can just make your fix, open up a pull request, and everything can be handled from there on the fly.

# Coding style

The coding style is primarily enforced by clang-format for things that can be easily checked by an automatic program. That includes brace style, indentation, spacing, etc.

For things that clang-format can't cover, there are a few guidelines below but the golden rule is to try and match the surrounding code as much as possible. The code isn't perfectly consistent so you might see varying naming conventions, avoid adding code that is significantly different to the context.

* `auto` is only allowed for types that are prohibitive: STL iterators and lambdas. Everywhere else you should use the explicit type.
* Use `NULL` rather than `nullptr`.
* If an `if` or `else` requires braces, add braces to all cases, rather than leaving one 'bare' without braces and another with braces.
* Strings are always UTF-8, everywhere, unless they are wide strings which are only used for windows compatibility. Plain `char*` byte strings should be used everywhere in non-OS specific code, and are always treated as UTF-8 (never assume a string will be plain ASCII).
* Limit STL use to a minimum. Allowed uses are `std::map`, `std::function`, `std::set`, algorithms like `std::sort` and `std::lower_bound`, and type-traits type checks like `std::is_same`. Use `rdcarray` instead of `std::vector` and `rdcstr` instead of `std::string`.
* Don't use hungarian notation aside from `m_` for members. Some existing names will use `p` prefixes for pointers but it should be phased out in new code. The exception is when working with hooked functions where the official parameter names have hungarian notation.

# Planning larger changes

When making a larger change, there are more things to take into consideration. Your changes need to be compatible with the project on a larger scale, as well as making sure your development process can merge into the mainline development with other contributors and the project maintainer.

There are a few guidelines to follow to make sure that everyone can work together with as little friction as possible:

Be proactive about communication. You can always [email me](mailto:baldurk@baldurk.org) about anything RenderDoc related including work you are planning, or currently doing. You can also open an issue to discuss a change. Staying in communication particularly with me can head off problems at a much earlier stage - perhaps a design you were planning would conflict with the direction of the project or with a better idea of the whole picture I can suggest something that would be more appropriate. It's much better to have a conversation and avoid spending time doing work that will be rejected or require rewrites at PR stage.

Aim to merge your work to the main line in reasonably sized chunks. How big is a 'reasonably sized' chunk is debateable, but bear in mind that your code must be able to be reviewed. If in doubt you can always split the work into a smaller standalone chunk, but keeping any one PR under 1000 lines changed at the very most is a good mental limit. Keeping a large change on a branch means that you have to do more merges from the mainline to keep up to date, and increases the chance that your changes will diverge away from the project. The [LLVM developer policy](http://llvm.org/docs/DeveloperPolicy.html#incremental-development) describes this kind of workflow and its benefits much better than I can.

# Where to Start

There are always plenty of things to do, if you'd like to chip in! Check out the [issues](https://github.com/baldurk/renderdoc/issues) for outstanding feature requests.

If you have a change you'd like to see make it into mainline, create a fork of renderdoc, make your changes to a branch, and open a pull request on github. You can look around for instructions on that - it's pretty simple.

Have a clear idea of what your change is to do. This goes hand in hand with the above, but if your change involves a lot of work then it's better to split it up into smaller components that can be developed and merged individually, towards the larger goal. Doing this makes it more easily digestible for the rest of the people on the project as well as making it easier to review the changes when they land.

It's fine to land features one-by-one in different drivers. Historically there have been features that only worked on certain APIs so don't feel that you must implement any new feature on all APIs. At the same time, feature parity is a goal of the project so you should aim to implement features that can be later ported to other APIs where possible either by yourself or by others.

