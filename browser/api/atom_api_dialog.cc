// Copyright (c) 2013 GitHub, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "browser/api/atom_api_dialog.h"

#include <string>

#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "browser/api/atom_api_window.h"
#include "browser/message_box.h"
#include "browser/native_window.h"

namespace atom {

namespace api {

namespace {

base::FilePath V8ValueToFilePath(v8::Handle<v8::Value> path) {
  std::string path_string(*v8::String::Utf8Value(path));
  return base::FilePath::FromUTF8Unsafe(path_string);
}

}  // namespace

v8::Handle<v8::Value> ShowMessageBox(const v8::Arguments &args) {
  v8::HandleScope scope;

  if (!args[0]->IsNumber() ||  // type
      !args[1]->IsArray() ||   // buttons
      !args[2]->IsString() ||  // title
      !args[3]->IsString() ||  // message
      !args[4]->IsString())    // detail
    return node::ThrowTypeError("Bad argument");

  MessageBoxType type = (MessageBoxType)(args[0]->IntegerValue());

  std::vector<std::string> buttons;
  v8::Handle<v8::Array> v8_buttons = v8::Handle<v8::Array>::Cast(args[1]);
  for (uint32_t i = 0; i < v8_buttons->Length(); ++i)
    buttons.push_back(*v8::String::Utf8Value(v8_buttons->Get(i)));

  std::string title(*v8::String::Utf8Value(args[2]));
  std::string message(*v8::String::Utf8Value(args[3]));
  std::string detail(*v8::String::Utf8Value(args[4]));

  int chosen = atom::ShowMessageBox(type, buttons, title, message, detail);
  return scope.Close(v8::Integer::New(chosen));
}

FileDialog::FileDialog(v8::Handle<v8::Object> wrapper)
    : EventEmitter(wrapper),
      dialog_(ui::SelectFileDialog::Create(this, NULL)) {
}

FileDialog::~FileDialog() {
}

void FileDialog::FileSelected(const base::FilePath& path,
                              int index, void* params) {
  int* id = static_cast<int*>(params);

  base::ListValue args;
  args.AppendInteger(*id);
  args.AppendString(path.value());

  Emit("selected", &args);

  delete id;
}

void FileDialog::MultiFilesSelected(const std::vector<base::FilePath>& files,
                                    void* params) {
  int* id = static_cast<int*>(params);

  base::ListValue args;
  args.AppendInteger(*id);
  for (size_t i = 0; i < files.size(); i++)
    args.AppendString(files[i].value());

  Emit("selected", &args);

  delete id;
}

void FileDialog::FileSelectionCanceled(void* params) {
  int* id = static_cast<int*>(params);

  base::ListValue args;
  args.AppendInteger(*id);

  Emit("cancelled", &args);

  delete id;
}

// static
v8::Handle<v8::Value> FileDialog::New(const v8::Arguments &args) {
  v8::HandleScope scope;

  if (!args.IsConstructCall())
    return node::ThrowError("Require constructor call");

  new FileDialog(args.This());

  return args.This();
}

// static
v8::Handle<v8::Value> FileDialog::SelectFile(const v8::Arguments &args) {
  FileDialog* self = Unwrap<FileDialog>(args.This());
  if (!self)
    return node::ThrowError("The FileDialog object is corrupted");

  if (!args[0]->IsObject() ||  // window
      !args[1]->IsNumber() ||  // type
      !args[2]->IsString() ||  // title
      !args[3]->IsString() ||  // default_path
      !args[4]->IsArray() ||   // file_types
      !args[5]->IsNumber() ||  // file_type_index
      !args[6]->IsString() ||  // default_extension
      !args[7]->IsNumber())    // callback_id
    return node::ThrowTypeError("Bad argument");

  Window* window = Window::Unwrap<Window>(args[0]->ToObject());
  if (!window || !window->window())
    return node::ThrowError("Invalid window");

  gfx::NativeWindow owning_window = window->window()->GetNativeWindow();

  int type = args[1]->IntegerValue();
  std::string title(*v8::String::Utf8Value(args[2]));
  base::FilePath default_path(V8ValueToFilePath(args[3]));

  ui::SelectFileDialog::FileTypeInfo file_types;
  FillTypeInfo(&file_types, v8::Handle<v8::Array>::Cast(args[4]));

  int file_type_index = args[5]->IntegerValue();
  std::string default_extension(*v8::String::Utf8Value(args[6]));
  int callback_id = args[7]->IntegerValue();

  self->dialog_->SelectFile(
      (ui::SelectFileDialog::Type)(type),
      UTF8ToUTF16(title),
      default_path,
      file_types.extensions.size() > 0 ? &file_types : NULL,
      file_type_index,
      default_extension,
      owning_window,
      new int(callback_id));

  return v8::Undefined();
}

// static
void FileDialog::FillTypeInfo(ui::SelectFileDialog::FileTypeInfo* file_types,
                              v8::Handle<v8::Array> v8_file_types) {
  file_types->include_all_files = true;
  file_types->support_drive = true;

  for (uint32_t i = 0; i < v8_file_types->Length(); ++i) {
    v8::Handle<v8::Object> element = v8_file_types->Get(i)->ToObject();

    std::string description(*v8::String::Utf8Value(
        element->Get(v8::String::New("description"))));
    file_types->extension_description_overrides.push_back(
        UTF8ToUTF16(description));

    std::vector<base::FilePath::StringType> extensions;
    v8::Handle<v8::Array> v8_extensions = v8::Handle<v8::Array>::Cast(
        element->Get(v8::String::New("extensions")));

    for (uint32_t j = 0; j < v8_extensions->Length(); ++j) {
      std::string extension(*v8::String::Utf8Value(v8_extensions->Get(j)));
      extensions.push_back(extension);
    }
    file_types->extensions.push_back(extensions);
  }
}

// static
void FileDialog::Initialize(v8::Handle<v8::Object> target) {
  v8::HandleScope scope;

  v8::Local<v8::FunctionTemplate> t(v8::FunctionTemplate::New(FileDialog::New));
  t->InstanceTemplate()->SetInternalFieldCount(1);
  t->SetClassName(v8::String::NewSymbol("FileDialog"));

  NODE_SET_PROTOTYPE_METHOD(t, "selectFile", SelectFile);

  target->Set(v8::String::NewSymbol("FileDialog"), t->GetFunction());

  NODE_SET_METHOD(target, "showMessageBox", ShowMessageBox);
}

}  // namespace api

}  // namespace atom

NODE_MODULE(atom_browser_dialog, atom::api::FileDialog::Initialize)
