'''
	The parameter set is a generator class used to efficiently permute a list of
	parameters and thier available options whilst associating branching logic
	with parameter values.

	This logic is available via the the nesting of parameters and parameter sets
	under parameter values, and the availability of opaque parameters that allow
	a level of indirection for distinct value groups.

	Execute this as a standalone file to see some example output. The example code
	is at the bottom of this file.
'''

## -----------------------------------------------------------------------------

class ParameterSet(object):
	def __init__(self):
		self.parameters = []

	def add(self, param_or_name, param_args=None):
		if not param_args:
			assert isinstance(param_or_name, Parameter)
			self.parameters.append(param_or_name)
		else:
			assert isinstance(param_or_name, str)
			param_or_name = Parameter(param_or_name, param_args)
			self.parameters.append(param_or_name)
			
		return param_or_name

	def update(self, name, param_args):
		for p in self.parameters:
			if p.name == name:
				p.update(param_args)
				return

	def permute(self):
		while True:
			## Generate arg permutation
			res = self.pair()

			## Only output when we actually have something to output.
			if len(res):
				yield res

			if self.step():
				break

	def pair(self):
		out = []
		for param in self.parameters:
			out += param.pair()

		return out

	def step(self):
		## Walk from right to left iterating indexes as we go, maintain loop when each parameter
		## interated past its end.
		if not len(self.parameters):
			return True

		iterable_index = len(self.parameters) - 1
		while self.parameters[iterable_index].step():
			iterable_index -= 1
			if iterable_index < 0:
				break

		## Break out when we stepped before the beginning of parameters
		if iterable_index < 0:
			self.reset()
			return True

		return False

	def reset(self):
		for param in self.parameters:
			param.reset()

	def generate_list(self):
		return [x for x in self.permute()]

	def generate_cmdline(self, arg_prefix):
		return [" ".join(map(lambda y: f"{arg_prefix}{y[0]} {y[1]}", x)) for x in self.permute()]

class Parameter(object):
	def __init__(self, name, value_list):
		self.name			= name
		self.value_list		= [[x, None] for x in value_list]
		self.iteration		= 0

	def update(self, value_list):
		self.value_list		= [[x, None] for x in value_list]

	def set_subset(self, value_id, parameter):
		for property in self.value_list:
			if(property[0] == value_id):
				property[1] = parameter
				break

	def current_has_subset(self):
		subset = self.value_list[self.iteration][1]
		return (subset != None) and ((isinstance(subset, ParameterSet)) or (isinstance(subset, Parameter)))

	def step(self):
		if self.iteration >= len(self.value_list):
			raise "Invalid parameter, iteration is greater than number of properties available"

		if self.current_has_subset():
			## Step my subset first, when that has finished step self.
			subset = self.value_list[self.iteration][1]
			if subset.step():
				self.iteration += 1
		else:
			self.iteration += 1

		if self.iteration >= len(self.value_list):
			self.iteration = 0
			return True

		return False

	def pair(self):
		if self.iteration >= len(self.value_list):
			raise "Invalid parameter, iteration is greater than number of properties available"

		property = self.value_list[self.iteration]

		res = []

		if self.name != None:
			res += [(self.name, property[0])]

		if self.current_has_subset():
			subset = self.value_list[self.iteration][1]
			p = subset.pair()

			if len(p):
				res += p

		return res

	def reset(self):
		self.iteration = 0

		for property in self.value_list:
			if property[1] != None:
				property[1].reset()

## -----------------------------------------------------------------------------
## Examples
## -----------------------------------------------------------------------------

def print_perms(perms):
	print("[\n\t{0}\n]".format("\n\t".join(map(str, perms))))

def example_simple():
	pset = ParameterSet()
	pset.add(Parameter("x", ["val1", "val2"]))
	pset.add(Parameter("y", [25, 30]))
	perms = [x for x in pset.permute()]

	print("\nExample Simple")
	print_perms(perms)

def example_subparam_opaque():
	pset = ParameterSet()
	opaqueparam = Parameter(None, ["a", "b"])
	opaqueparam.set_subset("a", Parameter("test_a", [10, 20]))
	pset.add(opaqueparam)
	perms = [x for x in pset.permute()]

	print("\nExample subset with opaque parameter")
	print_perms(perms)

def example_subparam():
	pset = ParameterSet()
	param = Parameter("with_sub", ["a", "b"])
	param.set_subset("a", Parameter("test_a", [10, 20]))
	pset.add(param)
	perms = [x for x in pset.permute()]

	print("\nExample subset with parameter")
	print_perms(perms)

def example_subparamset_opaque():
	pset = ParameterSet()
	subpset = ParameterSet()

	subpset.add(Parameter("c", [0, 1]))
	subpset.add(Parameter("d", [0, 1]))

	opaqueparam = Parameter(None, ["a", "b"])
	opaqueparam.set_subset("a", subpset)

	pset.add(opaqueparam)
	perms = [x for x in pset.permute()]

	print("\nExample subset with opaque ParameterSet")
	print_perms(perms)

def example_subparamset():
	pset = ParameterSet()
	subpset = ParameterSet()
	subpset2 = ParameterSet()

	subpset.add(Parameter("c", [0, 1]))
	subpset.add(Parameter("d", [0, 1]))

	param = Parameter("with_sub", ["a", "b"])
	param.set_subset("a", subpset)
	param.set_subset("b", subpset2)

	pset.add(param)
	perms = [x for x in pset.permute()]

	print("\nExample subset with ParameterSet")
	print_perms(perms)

def example_complex():
	topset = ParameterSet()
	set0 = ParameterSet()
	set1 = ParameterSet()
	set2 = ParameterSet()
	set3 = ParameterSet()

	set0param0 = Parameter("set0_param0", [0, 1])
	set0param0.set_subset(0, set1)
	set0param0.set_subset(1, set2)
	set0.add(set0param0)

	set1.add(Parameter("set1_param0", [0, 1]))
	set1.add(Parameter("set1_param1", ["hello"]))

	set2opaque0 = Parameter(None, [0, 1])
	set2opaque0.set_subset(0, Parameter("set2_param0", [10]))
	set2opaque0.set_subset(1, Parameter("set2_param1", [20]))
	set2.add(set2opaque0)

	set2param1 = Parameter("set2_param1", [0, 1, 2])
	set2param1.set_subset(2, set3)
	set2.add(set2param1)

	set3.add(Parameter("set3param0", [0]))
	set3.add(Parameter("set3param1", [22]))
	set3.add(Parameter("set3param2", [33]))

	topparam = Parameter("normal", ["a", "b"])
	topparam.set_subset("b", set0)
	topset.add(topparam)

	perms = [x for x in topset.permute()]
	print("\nExample complex")
	print_perms(perms)

def main():
	example_simple()
	example_subparam_opaque()
	example_subparam()
	example_subparamset_opaque()
	example_subparamset()
	example_complex()

if __name__ == "__main__":
	main()

## -----------------------------------------------------------------------------