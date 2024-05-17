// Helpers used in macro generated code.

// Auto-deref based specialization for calling Resource::set_name. Ref:
// https://lukaskalbertodt.github.io/2019/12/05/generalized-autoref-based-specialization.html

pub struct ResourceNamer<T>(pub T);

pub trait ResourceNamerTrait {
    fn set_name(&self, name: &'static [i8]);
}

impl<T: crate::Resource> ResourceNamerTrait for &&ResourceNamer<T> {
    fn set_name(&self, name: &'static [i8]) {
        self.0.set_name(name);
    }
}

pub trait NonResourceNamerTrait {
    fn set_name(&self, _name: &'static [i8]) {}
}
impl<T> NonResourceNamerTrait for ResourceNamer<T>{}

#[macro_export]
#[doc(hidden)]
macro_rules! set_name_if_resource
{
    ($r:expr, $name:expr) => {
        {
            use $crate::support::{*};
            let r = ResourceNamer($r);
            (&&&r).set_name(unsafe{&*($name as *const [u8] as *const [i8])});
            r.0
        }
    }
}

#[macro_export]
#[doc(hidden)]
macro_rules! static_cstr {
    ($e:expr) => {
        match core::ffi::CStr::from_bytes_with_nul((concat!($e, "\0")).as_bytes()) { Ok(s) => s, _ => panic!("Invalid CStr!") }
    };
}
