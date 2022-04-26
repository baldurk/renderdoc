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
	result = cap.OpenFile(filename, '', None)

	# Make sure the file opened successfully
	if result != rd.ResultCode.Succeeded:
		raise RuntimeError("Couldn't open file: " + str(result))

	# Make sure we can replay
	if not cap.LocalReplaySupport():
		raise RuntimeError("Capture cannot be replayed")

	# Initialise the replay
	result,controller = cap.OpenCapture(rd.ReplayOptions(), None)

	if result != rd.ResultCode.Succeeded:
		raise RuntimeError("Couldn't initialise replay: " + str(result))

	return cap,controller

if 'pyrenderdoc' in globals():
	raise RuntimeError("This sample should not be run within the RenderDoc UI")
else:
	if len(sys.argv) <= 1:
		print('Usage: python3 {} filename.rdc'.format(sys.argv[0]))
		sys.exit(0)

	rd.InitialiseReplay(rd.GlobalEnvironment(), [])

	cap,controller = loadCapture(sys.argv[1])

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

# Fetch the list of actions
actions = controller.GetRootActions()

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

# Start on the first action
curact = actions[0]

loopcount = 0

# The advance function will be called every 50ms, to move to the next action
def advance():
	global out, window, curact, actions, loopcount

	# Move to the current action
	controller.SetFrameEvent(curact.eventId, False)

	# Initialise a default TextureDisplay object
	disp = rd.TextureDisplay()

	# Set the first colour output as the texture to display
	disp.resourceId = curact.outputs[0]

	if disp.resourceId != rd.ResourceId.Null():
		# Get the details of this texture
		texDetails = getTexture(disp.resourceId)

		# Calculate the scale required in width and height
		widthScale = window.winfo_width() / texDetails.width
		heightScale = window.winfo_height() / texDetails.height

		# Use the lower scale to fit the texture on the window
		disp.scale = min(widthScale, heightScale)

		# Update the texture display
		out.SetTextureDisplay(disp)

	# Set the next action
	curact = curact.next

	# If we have no next action, start again from the first
	if curact is None:
		loopcount = loopcount + 1
		curact = actions[0]

	# after 3 loops, quit
	if loopcount == 3:
		window.quit()
	else:
		window.after(50, advance)

# Start the callbacks
advance()
paint()

# Start the main window loop
window.mainloop()

controller.Shutdown()

cap.Shutdown()

if 'pyrenderdoc' not in globals():
	rd.ShutdownReplay()
