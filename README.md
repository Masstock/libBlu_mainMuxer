
# libBlu mainMuxer

**libBlu mainMuxer** (temporary name) is an experimental MPEG-2 Transport Stream muxer created to multiplex video, audio and other kind of elementary stream into the BDAV MPEG-2 Transport Stream format (.m2ts) suitable for Blu-ray Disc authoring.

## Presentation / Goal

Project first lines were written two years ago when I started looking at Blu-ray Disc authoring and learning  C programming. My main goal was to understand how does the format works, and to experiment with it. In my first attempt, I try to use the best currently available open-source MPEG-TS muxer: the well-named [tsMuxer](https://github.com/justdan96/tsMuxer). But because of my lack of C++ programming knowledge by then, I chose to start from scratch.

MPEG-2 Video and AC-3 (and others Dolby Audio) were the first supported formats (that's why these modules needs huge rewrittings), followed by H.264 video and DTS audio family. Blu-ray menus (called Interactive Graphics) and bitmap subtitles (Presentation Graphics) formats follows. And I spent the past months rewriting the core of the muxer to implement better scheduling decisions.

The reasons why I'm publicly releasing my code are that the project is in a situation where I made decisions without been able to know if these are good or really bad. I don't have any access to official specifications (so my decisions are based on public informations, comparative tests and assumptions) and even less testing software or material (at the exception of Elecard's Stream Eye soft for H.264 buffering model, but my 30 days free trial is finished now). Secondly, the only public release was one year ago so if I don't take the decision to release it now, I probably won't be more able to do it waiting another year.

The project has currently no licence because I would like to add some functionalities before letting it go freely.

## Main features

* MPEG-TS muxer targeting BDAV (Blu-ray Disc Audio Visual) specifications.

  * Implements the theorical demuxer defined in MPEG-TS specifications (ITU-T Rec. H.222) called T-STD buffer verifier in order to stricely avoid buffer overflows.

  * Uses input bitstreams modification scripts called ESMS (Elementary Stream Modification Script) to produce compliant output and speed-up parsing.

  * Takes instructions from a input META file, similar to tsMuxer META files.

* Support of MPEG-2 Video (H.262), H.264 (MPEG-4 Part 10/AVC), Dolby Audio (AC-3, E-AC-3, TrueHD), DTS Audio (DCA + extensions), HDMV streams (IG/PG)...

  * Format specific and Blu-ray compliance checks.

  * Verbose debugging output describing every readen field.

  * Input streams properties editing (changing FPS, level, profile in H.264 bitstreams, extract Core for Dolby and DTS...).

  * H.264 Hypothetical Decoder implementation.

  * DTS-HDMA Peak Bit-Rate (PBR) smoothing.

* Generation of Blu-ray menus (IG) from a XML file.

  * Takes an XML file and PNG graphics as input.

  * High quality color reduction, palettes generation...

  * Included XLS definition file to easly made new menus.

## Download and install

Current version is the 0.5, see NEWS file for releases changelogs.

See releases.

## Building

See BUILD.

## Usage

See the integrated help option (-h or --help). Basic operation is as follow:

> ./mainMuxer -i input.meta -o output.m2ts

TODO

## Docs

TODO