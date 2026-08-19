NOTES:

	This plugin is capable to decode from *.AUD, *.AUE and *.R* files of DTS Movie/Trailer Discs.
	This plugin is capable to encode to *.AUD files of DTS Movie/Trailer Discs.
	This plugin uses proprietary APT-x100 codec.


USE:

	Install foo_input_apt-x100.fb2k-component file and restart foobar. Then open *.AUD, *.AUE or *.R* files for playback.
	Setup appropriate options at File->Preferences->Advanced->Tools->APT-x100 Decoder.
	If DTS file has no header (see warning in View->Console) playback parameters are obtained from file name like
	R<reel>T<tracks>S<serial>.AUD (R1T5S1001.AUD, R2T6.AUD, T8.AUD, ...). With any other name plugin plays 5 tracks.

	To convert to *.AUD select needed tracks, click right mouse button, select Convert->AUD convert, fill out fields
	and press "Convert" button.


CHANGELOG:

	06/17/26:
	Version 0.4.4 - 6/8 DTS discrete channel assignment fixed.

	06/13/26:
	Version 0.4.3 - +6 dB LFE gain for 5 channel files.

	06/12/26:
	Version 0.4.2 - Discrete 8 channel assignment and Linkwitz-Riley crossover are fixed.

	06/10/26:
	Version 0.4.1 - "Use X channel as (X-1).1 discrete" options added.

	06/07/26:
	Version 0.4.0 - Experimental: Adjustable Linkwitz-Riley crossover, in/out floating point PCM in APT-x100 codec.

	06/04/26:
	Version 0.3.15 - Channel assignment fixed.

	06/01/26:
	Version 0.3.14 - .R{1-N} file extensions supported.

	05/31/26:
	Version 0.3.13 - Front gain added, surround gain fixed.

	05/30/26:
	Version 0.3.12 - x86 vesion should provide output equal to proprietary APT-x100 codec.

	05/25/26:
	Version 0.3.11 - LFE and surround gain fixed.

	10/22/25:
	Version 0.3.10 - .TXT reader fixed.

	02/13/25:
	Version 0.3.9 - "Use back for side channels" option added at File->Preferences->Advanced->Tools->APT-x100 Decoder.

	02/12/25:
	Version 0.3.8 - Side for Back channel substitution for APT-x100 encoder added.

	02/11/25:
	Version 0.3.7 - AUD file channel assignment fixed.

	12/18/24:
	Version 0.3.6 - DTS disc type by serial number detection added, LF/HF filters changed.

	12/09/24:
	Version 0.3.5 - Channel assignment changed, R<reel>.TXT cue processing added.

	12/05/24:
	Version 0.3.4 - Encode 2, 5, 6 or 8 track AUD file from any source.

	12/04/24:
	Version 0.3.3 - IIR filter fixed.
 
	11/30/24:
	Version 0.3.2 - Experimental: output 2, 5, 6 and 8 track AUD file.

	11/15/24:
	Version 0.3.1 - New SDK.

	05/20/24:
	Version 0.3.0 - Experimental: macOS support added.

	04/16/24:
	Version 0.2.0 - Experimental: x64 support added.

	03/13/24:
	Version 0.1.12 - New SDK.

	01/13/22:
	Version 0.1.11 - New SDK.

	04/16/20:
	Version 0.1.10 - Handle DTS files without headers.

	04/15/20:
	Version 0.1.9 - "Process LFE" and "LFE gain (dB)" options added.

	03/10/20:
	Version 0.1.8 - *.APX file playback added.

	03/09/20:
	Version 0.1.7 - Stereo track playback added.

	11/22/19:
	Version 0.1.6 - *.AUD file encoder added.

	04/29/19:
	Version 0.1.5 - DTS *.AUE to *.AUD file decoder added.

	02/16/19:
	Version 0.1.4 - Channel order for 5 channel tracks is reverted back to 0.1.1 version.

	01/26/19:
	Version 0.1.3 - Improper channel order after seeking fixed.

	03/26/18:
	Version 0.1.2 - 6 and 8 track playback added.

	06/15/14:
	Version 0.1.1 - The primal.


Maxim V.Anisiutkin <maxim.anisiutkin@gmail.com>
