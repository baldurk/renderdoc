# Filing bug reports

Sorry that you've run into a bug! I'd like to be able to fix it, but first you need to fill out a good bug report to ensure I can understand the problem.

:warning: **PLEASE NOTE: YOU MUST USE THE PROVIDED ISSUE TEMPLATE!** :warning:

When filing a bug please click 'Get started' next to the 'Bug report' entry. This document explains the sections in the issue template and what you should put in them.

# General guidance

You can't provide too much information, and it's quite common to provide too little information. The more information you can provide the better I can determine the bug.

Avoid the temptation to guess or assume what the problem is based on what you're seeing, unless you have dived into the code to find out. It's probably not the case that RenderDoc crashes if you read a texture in a pixel shader. Most graphics programs do that! It's probably something more specific than that which is hard to identify without debugging. It's fine not to know what the problem is because that's my job! Just give as much information as possible.

If you have time, testing the latest nightly build of RenderDoc is a good idea since the bug you're reporting may have been fixed already. It's also a good idea to update your graphics drivers as some problems are caused by out of date drivers. If the problem you're seeing is new and wasn't present in an older version of RenderDoc, it would be extremely helpful to know approximately when the problem started. You can download any historical nightly build to test with.

If you really want to go in-depth there is a lot of writing online about how to make good bug reports! The classic that covers a lot is [Asking smart questions](http://www.catb.org/esr/faqs/smart-questions.html).

Below is guidance for each section of the issue template.

# Description

In the description section you can explain what kind of problem you're running into. The golden rule for this is to describe what you actually see happening, and then describe what you'd expect to see instead.

The point here is to make it clear to anyone what the bug is, because unless you describe it not everyone may understand what the problem is. If you describe what you're trying to do and where the problem arises, compared to what you would expect to happen if everything worked, it's easier to understand.

This is mostly relevant for bugs where something doesn't behave right rather than if something crashes where it's clear what is going wrong, but it's still useful information.

# Steps to reproduce

This section is the most important one!

In order to be able to fix a bug I need to first reproduce it to understand what goes wrong. You need to describe the steps taken.

Be sure that your repro steps really are steps that anyone can follow. Most of the time these repro steps will include "load my capture" or "capture my application". That is totally fine, but if you say that then you **must** upload and share the capture or the application. If you don't then I can't follow the steps, and they are not useful!

Github has a file size limit for uploaded files. In most cases your capture or application will be too large, so you can use a free online service such as dropbox, google drive, mega, or others to share your files.

If you want to share your capture or application privately then please [email me](mailto:baldurk@baldurk.org?subject=RenderDoc%20bug) with it.

When you have no problems sharing these, please do so as soon as you open your issue. If you open the issue without any reproducing materials, then in many cases I'll have to reply to ask for them anyway!

If you cannot share your capture or application even privately, then that's understandable. If that's the case please say up front that you can't share any such materials. In this case you have to provide as much information as you possibly can about what your program is doing and where the problem begins - I'll have to start guessing what the problem might be and the more information I have to work with the better my guesses can be.

# Environment

Please update the environment section for at least the three items present, the RenderDoc version you are using, your OS, and the graphics API(s) that you are seeing the bug on.

For the RenderDoc version if you're using a nightly build include the date or commit hash for that nightly build, since the v1.X number is not unique.

Giving more details here such as your GPU and driver version can't hurt, but you must include the three above since this gives important information about where the problem might be.
