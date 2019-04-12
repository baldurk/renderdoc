import sys

# Import renderdoc if not already imported (e.g. in the UI)
if 'renderdoc' not in sys.modules and '_renderdoc' not in sys.modules:
	import renderdoc

# Alias renderdoc for legibility
rd = renderdoc

def loadCapture(filename):
	# Open a capture file handle
	cap = rd.OpenCaptureFile()

	# Open a particular file - see also OpenBuffer to load from memory
	status = cap.OpenFile(filename, '', None)

	# Make sure the file opened successfully
	if status != rd.ReplayStatus.Succeeded:
		raise RuntimeError("Couldn't open file: " + str(status))

	# Make sure we can replay
	if not cap.LocalReplaySupport():
		raise RuntimeError("Capture cannot be replayed")

	# Initialise the replay
	status,controller = cap.OpenCapture(None)

	if status != rd.ReplayStatus.Succeeded:
		raise RuntimeError("Couldn't initialise replay: " + str(status))

	return cap,controller

if 'pyrenderdoc' in globals():
	raise RuntimeError("This sample should not be run within the RenderDoc UI")
else:
	cap,controller = loadCapture('test.rdc')

# Use tkinter to create windows
import tkinter

# Create a simple window
window = tkinter.Tk()
window.geometry("1280x720")

# Create renderdoc windowing data.
winsystems = [rd.WindowingSystem(i) for i in controller.GetSupportedWindowSystems()]

# Pass window system specific data here, See:
# - renderdoc.CreateWin32WindowingData
# - renderdoc.CreateXlibWindowingData
# - renderdoc.CreateXCBWindowingData

# This example code works on windows as that's simple to integrate with tkinter
if not rd.WindowingSystem.Win32 in winsystems:
    raise RuntimeError("Example requires Win32 windowing system: " + str(winsystems))

windata = rd.CreateWin32WindowingData(int(window.frame(), 16))

# Create a texture output on the window
out = controller.CreateOutput(windata, rd.ReplayOutputType.Texture)

# Fetch the list of textures
textures = controller.GetTextures()

# Fetch the list of drawcalls
draws = controller.GetDrawcalls()

# Function to look up the texture descriptor for a given resourceId
def getTexture(texid):
	global textures
	for tex in textures:
		if tex.resourceId == texid:
			return tex
	return None

# Our paint function will be called ever 33ms, to display the output
def paint():
	global out, window
	out.Display()
	window.after(33, paint)

# Start on the first drawcall
curdraw = 0

# The advance function will be called every 150ms, to move to the next draw
def advance():
	global out, window, curdraw

	# Move to the current drawcall
	controller.SetFrameEvent(draws[curdraw].eventId, False)

	# Initialise a default TextureDisplay object
	disp = rd.TextureDisplay()

	# Set the first colour output as the texture to display
	disp.resourceId = draws[curdraw].outputs[0]

	# Get the details of this texture
	texDetails = getTexture(disp.resourceId)

	# Calculate the scale required in width and height
	widthScale = window.winfo_width() / texDetails.width
	heightScale = window.winfo_height() / texDetails.height

	# Use the lower scale to fit the texture on the window
	disp.scale = min(widthScale, heightScale)

	# Update the texture display
	out.SetTextureDisplay(disp)

	# Set the next drawcall
	curdraw = (curdraw + 1) % len(draws)

	window.after(150, advance)

# Start the callbacks
advance()
paint()

# Start the main window loop
window.mainloop()

controller.Shutdown()

cap.Shutdown()
