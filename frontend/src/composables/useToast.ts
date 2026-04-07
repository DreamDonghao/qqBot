/**
 * @file useToast.ts
 * @brief Toast 提示 composable
 */

import { ref, type Ref } from 'vue'

const toast: Ref<string> = ref('')
const toastError: Ref<boolean> = ref(false)
let toastTimer: ReturnType<typeof setTimeout> | null = null

export function useToast() {
  const showToast = (msg: string, isError: boolean = false): void => {
    toast.value = msg
    toastError.value = isError
    if (toastTimer) clearTimeout(toastTimer)
    toastTimer = setTimeout(() => {
      toast.value = ''
    }, 3000)
  }

  return { toast, toastError, showToast }
}