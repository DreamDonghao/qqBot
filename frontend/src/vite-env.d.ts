/// <reference types="vite/client" />

declare module '*.vue' {
    import type {DefineComponent} from 'vue'
    const component: DefineComponent<{}, {}, any>
    export default component
}

export interface QQConfig {
    accessToken: string
    selfQQNumber: number
    qqHttpHost: string
    botName: string
}

export interface LLMConfig {
    name?: string
    apiKey: string
    baseUrl: string
    path: string
    model: string
    maxTokens: number
    temperature: number
    topP: number
}

export interface KBConfig {
    apiKey: string
    baseUrl: string
    knowledgeDatasetId: string
    memoryDatasetId: string
    memoryDocumentId: string
}

export interface MemoryConfig {
    memoryTriggerCount: number
    memoryChatRecordLimit: number
    shortTermMemoryMax: number
    shortTermMemoryLimit: number
    memoryMigrateCount: number
}

export interface Emoji {
    name: string
    path: string
}

export interface Admin {
    qq: number
}

export interface Group {
    groupId: number
    groupName: string
    messageCount: number
    enabled?: boolean
}

export interface ChatMessage {
    id?: number
    role: 'user' | 'assistant'
    content: string
}

export interface ApiResponse {
    success: boolean
    error?: string
    data?: any
}

export interface CustomTool {
    id: number
    name: string
    description: string
    parameters: string  // JSON Schema 字符串
    executorType: 'python' | 'http'
    executorConfig: string  // JSON 配置字符串
    enabled: boolean
}