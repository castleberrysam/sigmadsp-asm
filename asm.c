#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/debug-helpers.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static MonoDomain *domain;
static MonoClass *class_arraylist;
static MonoClass *class_hashtable;
static MonoClass *class_asmdocs;
static MonoClass *class_asmprocess;

static MonoImage *open_dll(const char *filename);
static MonoClass *find_class(MonoImage *image, const char *package, const char *classname);
static MonoMethod *find_method(MonoClass *class, const char *methodname, int num_args);
static void set_field(MonoObject *obj, const char *fieldname, void *value);
static void get_field(MonoObject *obj, const char *fieldname, void *value);

static void remove_newline(char *str);
static void print_class_methods(MonoClass *class);

static MonoObject *arraylist_new(void);
static MonoObject *arraylist_add(MonoObject *arraylist, MonoObject *obj);

static MonoObject *asmprocess_new(bool debug, const char *dsp, uint32_t wordlen, bool standalone, const char *path);
static MonoObject *asmprocess_detvalidcodeline(MonoObject *asmprocess, MonoObject *code_and_nets);
static MonoObject *asmprocess_decode(MonoObject *asmprocess, MonoObject *error_and_proc);

static MonoObject *asmdocs_new(void);

int main(int argc, char **argv)
{
  if(argc < 2) {
    printf("Usage: %s <assembly file> [netlist file]\n", argv[0]);
    exit(1);
  }

  // open files
  FILE *file_asm = fopen(argv[1], "r");
  if(!file_asm) {
    printf("Failed to open assembly file.\n");
    exit(1);
  }

  FILE *file_net = NULL;
  if(argc > 2) {
    file_net = fopen(argv[2], "r");
    if(!file_net) {
      printf("Failed to open netlist file.\n");
      exit(1);
    }
  }

  // enable MONO_IOMAP
  putenv("MONO_IOMAP=all");

  // create domain
  domain = mono_jit_init("sigmadsp-asm");

  // load libraries
  MonoImage *image_corlib = mono_get_corlib();
  MonoImage *image_sigma = open_dll("SigmaCompiler.dll");
  MonoImage *image_sigma100 = open_dll("Sigma100Compiler.dll");

  // get classes
  class_arraylist = find_class(image_corlib, "System.Collections", "ArrayList");
  class_hashtable = find_class(image_corlib, "System.Collections", "Hashtable");
  class_asmdocs = find_class(image_sigma, "ADICtrls", "AssemblyDocuments");
  class_asmprocess = find_class(image_sigma100, "Sigma100_Comp", "AssemblyProcess100");

  // make objects
  MonoObject *asmprocess = asmprocess_new(true, "SIGMA100", 5, false, "output/");
  MonoObject *code_and_nets = arraylist_new();

  MonoObject *code_and_net = asmdocs_new();
  arraylist_add(code_and_nets, code_and_net);

  // load assembly code and net list
  MonoObject *code, *nets, *params;
  get_field(code_and_net, "Code", &code);
  get_field(code_and_net, "Nets", &nets);
  get_field(code_and_net, "Params", &params);

  char buf[1024];
  while(fgets(buf, sizeof(buf), file_asm)) {
    remove_newline(buf);
    arraylist_add(code, (MonoObject *) mono_string_new(domain, buf));
  }
  fclose(file_asm);

  if(file_net) {
    while(fgets(buf, sizeof(buf), file_net)) {
      remove_newline(buf);
      arraylist_add(nets, (MonoObject *) mono_string_new(domain, buf));
    }
    fclose(file_net);
  }

  // assemble!
  MonoObject *error_and_proc = asmprocess_detvalidcodeline(asmprocess, code_and_nets);
  MonoObject *binary = asmprocess_decode(asmprocess, error_and_proc);
}

static MonoImage *open_dll(const char *filename)
{
  MonoImageOpenStatus status;
  MonoAssembly *assembly = mono_assembly_open(filename, &status);
  if(!assembly) {
    printf("Loading library %s failed.\n", filename);
    exit(1);
  }

  return mono_assembly_get_image(assembly);
}

static MonoClass *find_class(MonoImage *image, const char *package, const char *classname)
{
  MonoClass *class = mono_class_from_name(image, package, classname);
  if(!class) {
    printf("Finding class %s in namespace %s failed.\n", classname, package);
    exit(1);
  }

  return class;
}

static MonoMethod *find_method(MonoClass *class, const char *methodname, int num_args)
{
  MonoMethod *method = mono_class_get_method_from_name(class, methodname, num_args);
  if(!method) {
    printf("Finding method %s in class %s failed.\n", methodname, mono_class_get_name(class));
    exit(1);
  }

  return method;
}

static void set_field(MonoObject *obj, const char *fieldname, void *value)
{
  MonoClass *class = mono_object_get_class(obj);
  MonoClassField *field = mono_class_get_field_from_name(class, fieldname);
  if(!field) {
    printf("Finding field %s in class %s failed.\n", fieldname, mono_class_get_name(class));
    exit(1);
  }

  mono_field_set_value(obj, field, value);
}

static void get_field(MonoObject *obj, const char *fieldname, void *value)
{
  MonoClass *class = mono_object_get_class(obj);
  MonoClassField *field = mono_class_get_field_from_name(class, fieldname);
  if(!field) {
    printf("Finding field %s in class %s failed.\n", fieldname, mono_class_get_name(class));
    exit(1);
  }

  mono_field_get_value(obj, field, value);
}

static void remove_newline(char *str)
{
  size_t len = strlen(str);
  if(str[len-1] == '\n') {
    if(str[len-2] == '\r') {
      str[len-2] = '\0';
    } else {
      str[len-1] = '\0';
    }
  }
}

static void print_class_methods(MonoClass *class)
{
  void *iter = NULL;
  MonoMethod *method;
  while((method = mono_class_get_methods(class, &iter))) {
    printf("%s\n", mono_method_full_name(method, 1));
  }
}

static MonoObject *arraylist_new(void)
{
  MonoObject *arraylist = mono_object_new(domain, class_arraylist);
  mono_runtime_object_init(arraylist);
  return arraylist;
}

static MonoObject *arraylist_add(MonoObject *arraylist, MonoObject *obj)
{
  static MonoMethod *method;
  if(!method) {method = find_method(class_arraylist, "Add", 1);}

  void *args[1];
  args[0] = obj;

  return mono_runtime_invoke(method, arraylist, args, NULL);
}

static MonoObject *asmprocess_new(bool debug, const char *dsp, uint32_t wordlen, bool standalone, const char *path)
{
  static MonoMethod *method;
  if(!method) {method = find_method(class_asmprocess, ".ctor", 5);}

  void *args[5];
  args[0] = &debug;
  args[1] = mono_string_new(domain, dsp);
  args[2] = &wordlen;
  args[3] = &standalone;
  args[4] = mono_string_new(domain, path);

  MonoObject *asmprocess = mono_object_new(domain, class_asmprocess);
  mono_runtime_invoke(method, asmprocess, args, NULL);
  return asmprocess;
}

static MonoObject *asmprocess_detvalidcodeline(MonoObject *asmprocess, MonoObject *code_and_nets)
{
  static MonoMethod *method;
  if(!method) {method = find_method(class_asmprocess, "DetermineValidCodeLine_opt", 1);}

  void *args[1];
  args[0] = code_and_nets;

  return mono_runtime_invoke(method, asmprocess, args, NULL);
}

static MonoObject *asmprocess_decode(MonoObject *asmprocess, MonoObject *error_and_proc)
{
  static MonoMethod *method;
  if(!method) {method = find_method(class_asmprocess, "Decode", 1);}

  void *args[1];
  args[0] = error_and_proc;

  return mono_runtime_invoke(method, asmprocess, args, NULL);
}

static MonoObject *asmdocs_new(void)
{
  MonoObject *asmdocs = mono_object_new(domain, class_asmdocs);
  mono_runtime_object_init(asmdocs);
  return asmdocs;
}
